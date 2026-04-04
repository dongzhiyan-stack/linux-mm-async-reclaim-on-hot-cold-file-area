// SPDX-License-Identifier: GPL-2.0-only
/*
 *	linux/mm/filemap.c
 *
 * Copyright (C) 1994-1999  Linus Torvalds
 */

/*
 * This file handles the generic file mmap semantics used by
 * most "normal" filesystems (but you don't /have/ to use this:
 * the NFS filesystem used to do this differently, for example)
 */
#include <linux/export.h>
#include <linux/compiler.h>
#include <linux/dax.h>
#include <linux/fs.h>
#include <linux/sched/signal.h>
#include <linux/uaccess.h>
#include <linux/capability.h>
#include <linux/kernel_stat.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/syscalls.h>
#include <linux/mman.h>
#include <linux/pagemap.h>
#include <linux/file.h>
#include <linux/uio.h>
#include <linux/error-injection.h>
#include <linux/hash.h>
#include <linux/writeback.h>
#include <linux/backing-dev.h>
#include <linux/pagevec.h>
#include <linux/security.h>
#include <linux/cpuset.h>
#include <linux/hugetlb.h>
#include <linux/memcontrol.h>
#include <linux/shmem_fs.h>
#include <linux/rmap.h>
#include <linux/delayacct.h>
#include <linux/psi.h>
#include <linux/ramfs.h>
#include <linux/page_idle.h>
#include <linux/migrate.h>
#include <linux/pipe_fs_i.h>
#include <linux/splice.h>
#include <linux/rcupdate_wait.h>
#include <linux/sched/mm.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>
#include "internal.h"

#include "async_memory_reclaim_for_cold_file_area.h"
#include <linux/version.h>
static void mapping_set_update(struct xa_state *xas,
		struct address_space *mapping)
{
	if (dax_mapping(mapping) || shmem_mapping(mapping))
		return;
	xas_set_update(xas, workingset_update_node);
	xas_set_lru(xas, &shadow_nodes);
}
void page_cache_delete_for_file_area(struct address_space *mapping,
		struct folio *folio, void *shadow)
{
	XA_STATE(xas, &mapping->i_pages, folio->index >>PAGE_COUNT_IN_AREA_SHIFT);
	long nr = 1;
	struct file_area *p_file_area; 
	struct file_stat_base *p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;

	//令page索引与上0x3得到它在file_area的pages[]数组的下标
	unsigned int page_offset_in_file_area = folio->index & PAGE_COUNT_IN_AREA_MASK;

	mapping_set_update(&xas, mapping);//xarray shadow 的处理，先不管


	if(folio_nr_pages(folio) > 1){
		panic("%s folio_nr_pages:%ld\n",__func__,folio_nr_pages(folio));
	}
#if 0	
	xas_set_order(&xas, folio->index, folio_order(folio));
	nr = folio_nr_pages(folio);	
#endif
	VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);

	p_file_area = xas_load(&xas);
	if(!p_file_area || !is_file_area_entry(p_file_area))
		panic("%s mapping:0x%llx folio:0x%llx file_area:0x%llx\n",__func__,(u64)mapping,(u64)folio,(u64)p_file_area);

	p_file_area = entry_to_file_area(p_file_area);
	if(folio != rcu_dereference(p_file_area->pages[page_offset_in_file_area])){
		panic("%s mapping:0x%llx folio:0x%llx != p_file_area->pages:0x%llx\n",__func__,(u64)mapping,(u64)folio,(u64)p_file_area->pages[page_offset_in_file_area]);
	}
	/*1:kswapd进程内存回收 2:进程直接内存回收 shadow非NULL。 3:shadow是NULL，但current->mm非NULL，说明是文件截断
	 * 剩下的就是异步内存回收线程，该if才成立*/
	if(shadow)
		hot_cold_file_global_info.kswapd_free_page_count ++;
	else if(!current->mm){
		hot_cold_file_global_info.async_thread_free_page_count ++;

		/* 最新方案，异步内存回收page时，file_area_state的shadow bit不再使用。而是把1赋值给file_area->pages[]。这样bit0是1
		 * 说明是个shadow entry，但是kswapd内存回收的的shadow又远远大于1，依此可以区分该page是被异步内存回收还是kswapd。*/
		shadow = (void *)(0x1);
		if(strncmp(current->comm,"async_memory",12))
			printk("%s %s %d async memory errror!!!!!!!!!\n",__func__,current->comm,current->pid);
	}
	rcu_assign_pointer(p_file_area->pages[page_offset_in_file_area], shadow);

	FILE_AREA_PRINT1("%s mapping:0x%llx folio:0x%llx index:%ld p_file_area:0x%llx page_offset_in_file_area:%d\n",__func__,(u64)mapping,(u64)folio,folio->index,(u64)p_file_area,page_offset_in_file_area);

	folio->mapping = NULL;
	mapping->nrpages -= nr;
    
	smp_wmb();
	//清理这个page在file_area->file_area_statue的对应的bit位，表示这个page被释放了
	clear_file_area_page_bit(p_file_area,page_offset_in_file_area);
	/* 如果page在xarray tree有dirty、writeback、towrite mark标记，必须清理掉，否则将来这个槽位的再有新的page，
	 * 这些mark标记会影响已经设置了dirty、writeback、towrite mark标记的错觉，从而导致判断错误*/
	if(is_file_area_page_mark_bit_set(p_file_area,page_offset_in_file_area,PAGECACHE_TAG_DIRTY))
		clear_file_area_page_mark_bit(p_file_area,page_offset_in_file_area,PAGECACHE_TAG_DIRTY);
	if(is_file_area_page_mark_bit_set(p_file_area,page_offset_in_file_area,PAGECACHE_TAG_WRITEBACK))
		clear_file_area_page_mark_bit(p_file_area,page_offset_in_file_area,PAGECACHE_TAG_WRITEBACK);
	if(is_file_area_page_mark_bit_set(p_file_area,page_offset_in_file_area,PAGECACHE_TAG_TOWRITE))
		clear_file_area_page_mark_bit(p_file_area,page_offset_in_file_area,PAGECACHE_TAG_TOWRITE);

	//如果这个file_area还有page，直接返回。否则才会xas_store(&xas, NULL)清空这个file_area
	if(file_area_have_page(p_file_area))
		return;


	/*这里有个隐藏很深的坑?????????在file_area的page都释放后，file_area还要一直停留在xarray tree。因为后续如果file_area的page又被
	 *访问了，而对应xarray tree的槽位已经有file_area，依据这点判定发生了refault，file_area是refault file_area，后续长时间不再回收
	 file_area的page。故正常情况file_area的page全被释放了但file_area不能从xarray tree剔除。只有下边两种情况才行
     1:file_area的page被释放后，过了很长时间，file_area的page依然没人访问，则异步内存回收线程释放掉file_area结构，并把file_area从xarray tree剔除
     2:该文件被iput()释放inode了，mapping_exiting(maping)成立，此时执行到该函数也要把没有page的file_area从xarray tree剔除
    */
	if(mapping_exiting(mapping)){
		void *old_entry = xas_store(&xas, NULL);

		/* 可能存在一个并发，kswapd执行page_cache_delete_for_file_area()释放page，业务进程iput()释放文件执行 find_get_entries_for_file_area()
		 * 二者都会xas_store(&xas, NULL)把file_area从xarray tree剔除，但是只有成功把file_area从xarry tree剔除，返回值old_entry
		 * 非NULL，才能把file_area移动到global_file_stat.file_area_delete_list链表，依次防护重复把file_area移动到file_area_delete_list链表*/
		if(/*p_file_area->mapping && old_entry && */file_stat_in_global_base((struct file_stat_base *)mapping->rh_reserved1)){
			/*该函数全程xas_lock加锁，并且最开头可以从xarray tree查找到file_area，这里不可能从xarray tree查不到file_area*/
			if(!old_entry)
				panic("%s mapping:0x%llx p_file_area:0x%llx file_area_state:0x%x error old_entry NULL\n",__func__,(u64)mapping,(u64)p_file_area,p_file_area->file_area_state);
			/*可能并发iput()执行find_get_entry_for_file_area()把file_area->mapping置NULL了，这个没有xas_lock加锁防护*/
		    if(p_file_area->mapping){
				/*p_file_area->mapping置NULL，表示该文件iput了，马上要释放inode和mapping了*/
				WRITE_ONCE(p_file_area->mapping, 0);
				/*这个写屏障保证异步内存回收线程cold_file_area_delete()函数里，立即看到file_area->mapping是NULL*/
				smp_wmb();
				set_file_area_in_mapping_delete(p_file_area);
			}else{
				printk("%s file_area:0x%llx mapping NULL\n",__func__,(u64)p_file_area);
			}
		}
	}

	xas_init_marks(&xas);
}
EXPORT_SYMBOL(page_cache_delete_for_file_area);
void page_cache_delete_batch_for_file_area(struct address_space *mapping,
		struct folio_batch *fbatch)
{
	XA_STATE(xas, &mapping->i_pages, fbatch->folios[0]->index >> PAGE_COUNT_IN_AREA_SHIFT);
	long total_pages = 0;
	int i = 0;
	struct folio *folio;
	struct file_area *p_file_area;
	//令page索引与上0x3得到它在file_area的pages[]数组的下标
	unsigned int page_offset_in_file_area = fbatch->folios[0]->index & PAGE_COUNT_IN_AREA_MASK;
	struct file_stat_base *p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;

	//mapping_set_update(&xas, mapping); 不需要设置shadow operation

	xas_for_each(&xas, p_file_area, ULONG_MAX) {
		if(!p_file_area || !is_file_area_entry(p_file_area))
			panic("%s mapping:0x%llx p_file_area:0x%llx NULL\n",__func__,(u64)mapping,(u64)p_file_area);
		p_file_area = entry_to_file_area(p_file_area);

find_page_from_file_area:
		folio = rcu_dereference(p_file_area->pages[page_offset_in_file_area]);
		/*如果fiolio是file_area的索引，则对folio清NULL，避免folio干扰后续判断*/
		folio_is_file_area_index_or_shadow_and_clear_NULL(folio);

		if(!folio){
			goto next_page;
		}
		if(!file_area_have_page(p_file_area))
			panic("%s file_area:0x%llx folio:0x%llx page_offset_in_file_area:%d mapping:0x%llx_0x%llx file_area_have_page error\n",__func__,(u64)p_file_area,(u64)folio,page_offset_in_file_area,(u64)mapping,(u64)((folio)->mapping));

		/*检测查找到的page是否正确，不是则crash*/
		CHECK_FOLIO_FROM_FILE_AREA_VALID(&xas,mapping,folio,p_file_area,page_offset_in_file_area,((xas.xa_index << PAGE_COUNT_IN_AREA_SHIFT) + page_offset_in_file_area));

		if (i >= folio_batch_count(fbatch))
			break;

		/* A swap/dax/shadow entry got inserted? Skip it. */
		if (xa_is_value(folio) || folio_nr_pages(folio) > 1){
			panic("%s folio:0x%llx xa_is_value folio_nr_pages:%ld\n",__func__,(u64)folio,folio_nr_pages(folio));
		}
		/*
		 * A page got inserted in our range? Skip it. We have our
		 * pages locked so they are protected from being removed.
		 * If we see a page whose index is higher than ours, it
		 * means our page has been removed, which shouldn't be
		 * possible because we're holding the PageLock.
		 */
		if (folio != fbatch->folios[i]) {
			VM_BUG_ON_FOLIO(folio->index >
					fbatch->folios[i]->index, folio);

			//这里主要是检查新的folio 和 它在file_area->file_area_state 中的是否设置了bit位，如果状态错了，panic
			if(folio && !is_file_area_page_bit_set(p_file_area,page_offset_in_file_area))
				panic("%s mapping:0x%llx p_file_area:0x%llx file_area_state:0x%x error\n",__func__,(u64)mapping,(u64)p_file_area,p_file_area->file_area_state);

			goto next_page;
		}

		WARN_ON_ONCE(!folio_test_locked(folio));

		folio->mapping = NULL;
		/* Leave folio->index set: truncation lookup relies on it */

		i++;
		rcu_assign_pointer(p_file_area->pages[page_offset_in_file_area], NULL);
		total_pages += folio_nr_pages(folio);
		FILE_AREA_PRINT1("%s mapping:0x%llx folio:0x%llx index:%ld p_file_area:0x%llx page_offset_in_file_area:%d\n",__func__,(u64)mapping,(u64)folio,folio->index,(u64)p_file_area,page_offset_in_file_area);

		smp_wmb();
		/*清理这个page在file_area->file_area_statue的对应的bit位，表示这个page被释放了*/
		clear_file_area_page_bit(p_file_area,page_offset_in_file_area);
		/* 如果page在xarray tree有dirty、writeback、towrite mark标记，必须清理掉，否则将来这个槽位的再有新的page，
		 * 这些mark标记会影响已经设置了dirty、writeback、towrite mark标记的错觉，从而导致判断错误*/
		if(is_file_area_page_mark_bit_set(p_file_area,page_offset_in_file_area,PAGECACHE_TAG_DIRTY))
			clear_file_area_page_mark_bit(p_file_area,page_offset_in_file_area,PAGECACHE_TAG_DIRTY);
		if(is_file_area_page_mark_bit_set(p_file_area,page_offset_in_file_area,PAGECACHE_TAG_WRITEBACK))
			clear_file_area_page_mark_bit(p_file_area,page_offset_in_file_area,PAGECACHE_TAG_WRITEBACK);
		if(is_file_area_page_mark_bit_set(p_file_area,page_offset_in_file_area,PAGECACHE_TAG_TOWRITE))
			clear_file_area_page_mark_bit(p_file_area,page_offset_in_file_area,PAGECACHE_TAG_TOWRITE);

		if(!file_area_have_page(p_file_area)){
			if(mapping_exiting(mapping)){
				/*page_cache_delete_batch_for_file_area()函数会循环遍历多个file_area。为了不干扰原生的xas，重新定义一个xas_del
				 *page_cache_delete_for_file_area不需要这样*/
				XA_STATE(xas_del, &mapping->i_pages, p_file_area->start_index); 
				void *old_entry = xas_store(&xas_del, NULL);

				if(/*p_file_area->mapping && old_entry && */file_stat_in_global_base((struct file_stat_base *)mapping->rh_reserved1)){
					/*该函数全程xas_lock加锁，并且最开头可以从xarray tree查找到file_area，这里不可能从xarray tree查不到file_area*/
					if(!old_entry)
						panic("%s mapping:0x%llx p_file_area:0x%llx file_area_state:0x%x error old_entry NULL\n",__func__,(u64)mapping,(u64)p_file_area,p_file_area->file_area_state);
					/*可能并发iput()执行find_get_entry_for_file_area()把file_area->mapping置NULL了，这个没有xas_lock加锁防护*/
		            if(p_file_area->mapping){
						/*p_file_area->mapping置NULL，表示该文件iput了，马上要释放inode和mapping了*/
						WRITE_ONCE(p_file_area->mapping, 0);
						/*这个写屏障保证异步内存回收线程cold_file_area_delete()函数里，立即看到file_area->mapping是NULL*/
						smp_wmb();
						set_file_area_in_mapping_delete(p_file_area);
					}else{
						printk("%s file_area:0x%llx mapping NULL\n",__func__,(u64)p_file_area);
					}
				}
			}
		}

next_page:
		page_offset_in_file_area ++;

		/*如果file_area里还有page没遍历到，goto find_page_from_file_area去查找file_area里的下一个page。否则到for循环开头
		 *xas_for_each()去查找下一个file_area，此时需要find_page_from_file_area清0，这个很关键*/
		if(page_offset_in_file_area < PAGE_COUNT_IN_AREA && file_area_have_page(p_file_area)){
			/*page_offset_in_file_area加1不能放到这里，重大逻辑错误。比如，上边判断page_offset_in_file_area是3的folio，
			 *然后执行到f(page_offset_in_file_area < PAGE_COUNT_IN_AREA)判断时，正常就应该不成立的，因为file_area的最后一个folio已经遍历过了*/
			goto find_page_from_file_area;
		}
		else
			page_offset_in_file_area = 0;
	}
	mapping->nrpages -= total_pages;
}
EXPORT_SYMBOL(page_cache_delete_batch_for_file_area);
bool filemap_range_has_page_for_file_area(struct address_space *mapping,
		loff_t start_byte, loff_t end_byte)
{
	XA_STATE(xas, &mapping->i_pages, (start_byte >> PAGE_SHIFT) >> PAGE_COUNT_IN_AREA_SHIFT);
	/*要查找的最后一个file_area的索引，有余数要加1。错了，不用加1，因为file_area的索引是从0开始*/
	pgoff_t max = (end_byte >> PAGE_SHIFT) >> PAGE_COUNT_IN_AREA_SHIFT;
	struct file_area* p_file_area;
	struct file_stat_base* p_file_stat_base;

	if (end_byte < start_byte)
		return false;

	rcu_read_lock();
	
	p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;
	smp_rmb();
	if(unlikely(!IS_SUPPORT_FILE_AREA_READ_WRITE(mapping)))
        printk("%s %s %d mapping:0x%llx file_stat:0x%lx has delete,do not use this file_stat!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",__func__,current->comm,current->pid,(u64)mapping,mapping->rh_reserved1);

	for (;;) {
		/*查找start_byte~end_byte地址范围内第一个有效的page对应的file_area，找不到返回NULL,然后下边return NULL。
		 *xas_find()会令xa.xa_offset自动加1*/
		p_file_area = xas_find(&xas, max);/*这里的max是要查询的最大file_area的索引，不是最大的page索引，很关键!!!!!!!!!*/
		if (xas_retry(&xas, p_file_area))
			continue;

		/* Shadow entries don't count */
		if (xa_is_value(p_file_area) || !is_file_area_entry(p_file_area)){
			panic("%s p_file_area:0x%llx xa_is_value\n",__func__,(u64)p_file_area);
		}
		if(!p_file_area)
			break;

		p_file_area = entry_to_file_area(p_file_area);
		/*重点，隐藏很深的问题，如果遇到有效的file_area但却没有page，那只能硬着头皮一直向后查找，直至找到max。
		 *这种情况是完全存在的，file_area的page全被回收了，但是file_area还残留着，file_area存在并不代表page存在!!!!!!!!!!*/
		if(!file_area_have_page(p_file_area)){
			continue;
		}
		/*
		 * We don't need to try to pin this page; we're about to
		 * release the RCU lock anyway.  It is enough to know that
		 * there was a page here recently.
		 */
		break;
	}
	rcu_read_unlock();
	FILE_AREA_PRINT("%s mapping:0x%llx p_file_area:0x%llx start_byte:%lld end_byte:%lld\n",__func__,(u64)mapping,(u64)p_file_area,start_byte,end_byte);


	/*file_area有page则返回1*/
	return  (p_file_area != NULL && file_area_have_page(p_file_area));
}
EXPORT_SYMBOL(filemap_range_has_page_for_file_area);
bool filemap_range_has_writeback_for_file_area(struct address_space *mapping,
		loff_t start_byte, loff_t end_byte)
{
	struct folio *folio;

	XA_STATE(xas, &mapping->i_pages, (start_byte >> PAGE_SHIFT) >> PAGE_COUNT_IN_AREA_SHIFT);
	/*要查找的最后一个file_area的索引，有余数要加1。错了，不用加1，因为file_area的索引是从0开始*/
	pgoff_t max = (end_byte >> PAGE_SHIFT) >> PAGE_COUNT_IN_AREA_SHIFT;
	struct file_area *p_file_area;
	struct file_stat_base *p_file_stat_base;
	pgoff_t max_page = (end_byte >> PAGE_SHIFT);
	//要查找的第一个page在file_area->pages[]数组里的偏移，令page索引与上0x3得到它在file_area的pages[]数组的下标
	unsigned int page_offset_in_file_area = (start_byte >> PAGE_SHIFT) & PAGE_COUNT_IN_AREA_MASK;
	unsigned long folio_index_from_xa_index;

	if (end_byte < start_byte)
		return false;

	rcu_read_lock();

	p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;
	smp_rmb();
	if(unlikely(!IS_SUPPORT_FILE_AREA_READ_WRITE(mapping)))
        printk("%s %s %d mapping:0x%llx file_stat:0x%lx has delete,do not use this file_stat!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",__func__,current->comm,current->pid,(u64)mapping,mapping->rh_reserved1);


	/*一个个查找start_byte~end_byte地址范围内的有效file_area并返回，一直查找max索引的file_area结束*/
	xas_for_each(&xas, p_file_area, max) {/*这里的max是要查询的最大file_area的索引，不是最大的page索引，很关键!!!!!!!!!*/
		if (xas_retry(&xas, p_file_area))
			continue;
		if (xa_is_value(p_file_area)){
			panic("%s page:0x%llx xa_is_value\n",__func__,(u64)p_file_area);
		}

		if(!is_file_area_entry(p_file_area))
			panic("%s mapping:0x%llx p_file_area:0x%llx  NULL\n",__func__,(u64)mapping,(u64)p_file_area);
		p_file_area = entry_to_file_area(p_file_area);

find_page_from_file_area:
		folio = (struct folio *)rcu_dereference(p_file_area->pages[page_offset_in_file_area]);
		/*如果page是file_area的索引，则对folio清NULL，避免folio干扰后续判断*/
		folio_is_file_area_index_or_shadow_and_clear_NULL(folio);
		/*page_cache_delete_batch()函数能进到这里folio一定不是NULL，但是现在无法保证，需要额外判定。但不能break，而是要去查找
		 *file_area里的下一个page。因为 xas_for_each()、xas_find()等函数现在从xarray tree查找的是file_area，不是page。只有
		 *找到的page是NULL，才能break结束查找*/
		if(!folio){
			goto next_page;
		}
		folio_index_from_xa_index = (xas.xa_index << PAGE_COUNT_IN_AREA_SHIFT) + page_offset_in_file_area;
		/* 如果有进程此时并发page_cache_delete_for_file_area()里释放该page，这个内存屏障，确保，看到的page不是NULL时，
		 * page在file_area->file_area_statue的对应的bit位一定是1，不是0*/
		smp_rmb();
		/*检测查找到的page是否正确，不是则crash*/
		CHECK_FOLIO_FROM_FILE_AREA_VALID(&xas,mapping,folio,p_file_area,page_offset_in_file_area,folio_index_from_xa_index);

		//超过了最大索引的page，则本次没有找到有效page
		if(folio_index_from_xa_index > max_page){
			folio = NULL;
			break;
		}

		if (folio_test_dirty(folio) || folio_test_locked(folio) ||
				folio_test_writeback(folio))
			break;

next_page:
		page_offset_in_file_area ++;

		/*如果file_area里还有page没遍历到，goto find_page_from_file_area去查找file_area里的下一个page。否则到for循环开头
		 *xas_for_each()去查找下一个file_area，此时需要对find_page_from_file_area清0*/
		if(page_offset_in_file_area < PAGE_COUNT_IN_AREA){
			/*page_offset_in_file_area加1不能放到这里，重大逻辑错误。比如，上边判断page_offset_in_file_area是3的folio，
			 *然后执行到f(page_offset_in_file_area < PAGE_COUNT_IN_AREA)判断时，正常就应该不成立的，因为file_area的最后一个folio已经遍历过了*/
			goto find_page_from_file_area;
		}
		else
			page_offset_in_file_area = 0;
	}
	rcu_read_unlock();
	FILE_AREA_PRINT("%s mapping:0x%llx p_file_area:0x%llx start_byte:%lld end_byte:%lld page:0x%llx\n",__func__,(u64)mapping,(u64)p_file_area,start_byte,end_byte,(u64)folio);

	return folio != NULL;
}
EXPORT_SYMBOL(filemap_range_has_writeback_for_file_area);
void replace_page_cache_folio_for_file_area(struct folio *old, struct folio *new)
{
	struct address_space *mapping = old->mapping;
	void (*free_folio)(struct folio *) = mapping->a_ops->free_folio;
	pgoff_t offset = old->index;
	XA_STATE(xas, &mapping->i_pages, offset >> PAGE_COUNT_IN_AREA_SHIFT);
	struct file_area *p_file_area;
	//令page索引与上0x3得到它在file_area的pages[]数组的下标
	unsigned int page_offset_in_file_area = offset & PAGE_COUNT_IN_AREA_MASK;

	VM_BUG_ON_FOLIO(!folio_test_locked(old), old);
	VM_BUG_ON_FOLIO(!folio_test_locked(new), new);
	VM_BUG_ON_FOLIO(new->mapping, new);

	folio_get(new);
	new->mapping = mapping;
	new->index = offset;

    mem_cgroup_replace_folio(old, new);

	xas_lock_irq(&xas);
	/*如果此时file_stat或者file_area cold_file_stat_delete()、cold_file_area_delete被释放了，那肯定是不合理的
	 *这里会触发panic*/
	if(unlikely(!IS_SUPPORT_FILE_AREA_READ_WRITE(mapping)))
        panic("%s %s %d mapping:0x%llx file_stat:0x%lx has delete,do not use this file_stat!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",__func__,current->comm,current->pid,(u64)mapping,mapping->rh_reserved1);

	p_file_area = (struct file_area *)xas_load(&xas);
	if(!p_file_area || !is_file_area_entry(p_file_area))
		panic("%s mapping:0x%llx p_file_area:0x%llx error\n",__func__,(u64)mapping,(u64)p_file_area);

	p_file_area = entry_to_file_area(p_file_area);
	if(old != (struct page *)rcu_dereference(p_file_area->pages[page_offset_in_file_area])){
		panic("%s mapping:0x%llx old:0x%llx != p_file_area->pages:0x%llx\n",__func__,(u64)mapping,(u64)old,(u64)p_file_area->pages[page_offset_in_file_area]);
	}
	rcu_assign_pointer(p_file_area->pages[page_offset_in_file_area],new);	
	FILE_AREA_PRINT1("%s mapping:0x%llx p_file_area:0x%llx old:0x%llx fnew:0x%llx page_offset_in_file_area:%d\n",__func__,(u64)mapping,(u64)p_file_area,(u64)old,(u64)fnew,page_offset_in_file_area);

	old->mapping = NULL;
	/* hugetlb pages do not participate in page cache accounting. */
	if (!folio_test_hugetlb(old))
		__lruvec_stat_sub_folio(old, NR_FILE_PAGES);
	if (!folio_test_hugetlb(new))
		__lruvec_stat_add_folio(new, NR_FILE_PAGES);
	if (folio_test_swapbacked(old))
		__lruvec_stat_sub_folio(old, NR_SHMEM);
	if (folio_test_swapbacked(new))
		__lruvec_stat_add_folio(new, NR_SHMEM);
	xas_unlock_irq(&xas);
	if (free_folio)
		free_folio(old);
	folio_put(old);
}
EXPORT_SYMBOL(replace_page_cache_folio_for_file_area);
static void inline async_and_kswapd_refault_page_count(struct file_stat_base *p_file_stat_base,struct folio *folio_temp,void **shadowp)
{
	/* 统计发生refault的page数，跟workingset_refault_file同一个含义。但是有个问题，只有被异步内存回收线程
	 * 回收的page的file_area才会被标记in_free，被kswapd内存回收的page的file_area，就不会标记in_free
	 * 了。问题就出在这里，当这些file_area的page将来被访问，发生refault，但是这些file_area因为没有in_free
	 * 标记，导致这里if不成立，而无法统计发生refault的page，漏掉了。怎么解决，kswapd内存回收的page最终
	 * 也是执行page_cache_delete_for_file_area函数释放page的，在该函数里，如果file_area没有in_free标记，
	 * 则标记in_free。后续该file_area的page再被访问，这里就可以统计到了。
	 *
	 * 有个问题，假设file_area里有3个page，只有一个page0内存回收成功，还有两个page，page1、page3没回收
	 * 成功。file_area被设置了in_free标记。如果将来page1被访问了，这里file_area_refault_file岂不是要
	 * 加1了，这就是误加1了，因为page1并没有发生refault。仔细想想不会，因为page1存在于file_area，将来
	 * 该page被访问，直接从xrray tree找到file_area再找到page1，就返回了，不会执行到当前函数。即便执行
	 * 到当前函数，因为page1存在于file_area，上边xas_set_err(&xas, -EEXIST)就返回了，不会执行到这里的
	 * hot_cold_file_global_info.file_area_refault_file ++。*/
	/*filemap_add_folio()函数增加refault次数有限制条件，就是gfp没有__GFP_WRITE标记，即folio不是write的，而是read的*/

	/*如果该page被异步内存回收线程回收而做了shadow标记*/
	if(1 == (u64)folio_temp){
		/*异步内存回收造成的refault page数统计到/proc/vmstat里*/
		atomic_long_add(1, &vm_node_stat[WORKINGSET_REFAULT_FILE]);

		/* file_area_refault_file存在多进程并发加1的情况而不准。这只是一个粗略的统计值，只要跟WORKINGSET_REFAULT_FILE统计的refault别偏差太大就行*/
		hot_cold_file_global_info.file_area_refault_file ++;
		if(p_file_stat_base->refault_page_count < USHRT_MAX - 2)
			p_file_stat_base->refault_page_count ++;

	}else{
		/*否则folio_temp非NULL，说明是kswapd内存的page保存的shadow。如果是文件截断回收的page则folio_temp是NULL。但是上边加了if(folio_temp)限制*/
		if(shadowp)
			*shadowp = folio_temp;

		/*这是统计kswapd造成的refault page数，有意义吗，反正我也统计不了呀。算了还是先统计吧*/
		hot_cold_file_global_info.kswapd_file_area_refault_file ++;
		if(p_file_stat_base->refault_page_count_last < USHRT_MAX - 2)
			p_file_stat_base->refault_page_count_last ++;
	}
}
/* 有些writeonly文件内存回收后，file_area移动到了file_stat->free链表，这种file_area有几千个。后续这些file_area又被访问了，
 * 属于write page refault。这些有大量page的file-area零散的分布在file_stat->free链表各处，导致内存紧张时，很难连续的从file_stat->free
 * 链表遍历到这些file_area并回收page，强制把file_stat->free链表遍历一遍，浪费cpu。于是想到这些write refalt的file_area，再add_folio把
 * page添加到file_area时，直接把file_area移动到file_stat->free链表尾。但是我不想使用file_stat_lock，浪费性能。但是异步内存回收线程
 * 此时也会向file_stat->free链表移入file_area，或者把file_stat->free链表上的file_area移动到file_stat->refault链表，或者释放掉
 * file_area且把file_area从file_stat->free链表剔除。不加锁也有解决办法。异步内存回收线程有这些操作时，
 * while(test_and_set_bit(file_area_stat,F_file_stat_in_move_free_list_file_area))
 *     msleep(1);
 *
 *    1:把file_aeea移动到file_stat->free链表
 *    2:把file_area移动到其他file_stat->refault链表
 *    3:把file-area从file_stat->free链表移除掉并释放
 *  clear_file_area_in_move_free_list_file_area
 *
 * add_folio函数里
 * if(0 == test_and_set_bit(file_area_stat,F_file_stat_in_move_free_list_file_area)){
 *     把write page refault的file_area移动到file_stat->free链表尾
 *     clear_file_area_in_move_free_list_file_area
 * }
 * 异步内存回收线程 跟 add_folio函数的线程，test_and_set_bit(file_area_stat,F_file_stat_in_move_free_list_file_area)抢占锁
 * 如果异步内存回收线程抢占成功，则add_folio不再把该file_area移动到file_stat->free链表尾。如果add_folio的线程先抢占成功，则
 * 该file_area移动到file_stat->free链表尾。此时异步内存回收线程msleep(1)休眠，避免并发操作file_stat->free链表的file_area
 */
inline void move_writeonly_file_area_to_free_list_tail(struct file_stat_base *p_file_stat_base,struct file_area *p_file_area)
{
    if(file_stat_in_file_stat_writeonly_file_head_list_base(p_file_stat_base) &&
			0 == test_and_set_bit(F_file_stat_in_move_free_list_file_area,(void *)(&p_file_stat_base->file_stat_status))){

	    struct file_stat *p_file_stat = container_of(p_file_stat_base,struct file_stat,file_stat_base);
		/*必须得保证异步内存回收线程先设置file_area的in_free标记，再把file_area移动到file_stat->free链表*/
		if(!file_area_in_free_list(p_file_area))
			panic("%s file_stat:0x%llx  status:0x%x file_area:0x%llx state:0x%x file_area not in_free\n",__func__,(u64)p_file_stat_base,p_file_stat_base->file_stat_status,(u64)p_file_area,p_file_area->file_area_state);

        smp_rmb();

		list_move_tail(&p_file_area->file_area_list,&p_file_stat->file_area_free);
		clear_file_stat_in_move_free_list_file_area_base(p_file_stat_base);
    }
}
noinline int __filemap_add_folio_for_file_area(struct address_space *mapping,
		struct folio *folio, pgoff_t index, gfp_t gfp, void **shadowp)
{
	/*index是long型？area_index_for_page也有必要定义成long型吧???????????????*/
	unsigned int area_index_for_page = index >> PAGE_COUNT_IN_AREA_SHIFT;
	XA_STATE(xas, &mapping->i_pages, area_index_for_page);
	void *alloced_shadow = NULL;
	int alloced_order = 0;
	bool huge;
	long nr;
	struct file_stat_base *p_file_stat_base;
	struct file_area *p_file_area;
	struct folio *folio_temp;
	gfp_t gfp_ori = gfp;

	//令page索引与上0x3得到它在file_area的pages[]数组的下标
	unsigned int page_offset_in_file_area = index & PAGE_COUNT_IN_AREA_MASK;

	VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);
	VM_BUG_ON_FOLIO(folio_test_swapbacked(folio), folio);
	VM_BUG_ON_FOLIO(folio_order(folio) < mapping_min_folio_order(mapping),
			folio);
	mapping_set_update(&xas, mapping);

	VM_BUG_ON_FOLIO(index & (folio_nr_pages(folio) - 1), folio);
	/*xas_set_order()里会把page索引重新赋值给xas.xa_index，而xas.xa_index正确应该是file_area索引*/
	xas_set_order(&xas, area_index_for_page, folio_order(folio));
	huge = folio_test_hugetlb(folio);
	nr = folio_nr_pages(folio);

	gfp &= GFP_RECLAIM_MASK;
	folio_ref_add(folio, nr);
	folio->mapping = mapping;
	folio->index = index;

	if(nr != 1 || folio_order(folio) != 0){
		panic("%s index:%ld folio->index:%ld nr:%ld folio_order(folio):%d\n",__func__,index,folio->index,nr,folio_order(folio));
	}
	
	rcu_read_lock();
	smp_rmb();
	for (;;) {
		int order = -1, split_order = 0;
		void *entry, *old = NULL;
		/*这个赋值NULL必须，因为这里可能第1次把folio添加到xarray tree失败，然后第2次这里赋值NULL 就受上一个page的影响了*/
		folio_temp = NULL;

		xas_lock_irq(&xas);
		/*file_stat可能会被方法删除，则分配一个新的file_stat，具体看cold_file_stat_delete()函数*/
		if(SUPPORT_FILE_AREA_INIT_OR_DELETE == READ_ONCE(mapping->rh_reserved1)){
			p_file_stat_base = file_stat_alloc_and_init_tiny_small(mapping,!mapping_mapped(mapping));

			if(!p_file_stat_base){
				xas_set_err(&xas, -ENOMEM);
				goto unlock;
			}
		}else
			p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;

		if(file_stat_in_delete_base(p_file_stat_base))
			panic("%s %s %d file_stat:0x%llx status:0x%x in delete\n",__func__,current->comm,current->pid,(u64)p_file_stat_base,p_file_stat_base->file_stat_status);
		
		xas_for_each_conflict(&xas, entry) {
			p_file_area = entry_to_file_area(entry);
			folio_temp = rcu_dereference(p_file_area->pages[page_offset_in_file_area]);
			if(folio_temp){
				if(!folio_is_file_area_index_or_shadow(folio_temp)){
					xas_set_err(&xas, -EEXIST);
					goto unlock;
				}

				/*统计refault page个数，但只有read的refault page才计入refault统计*/
				if(!(gfp_ori & __GFP_WRITE)){
					async_and_kswapd_refault_page_count(p_file_stat_base,folio_temp,shadowp);

					if((file_stat_in_test_base(p_file_stat_base) || is_global_file_stat_file_in_debug(mapping)) && (1 == (u64)folio_temp)){
						printk("%s refault file_stat:0x%llx file_area:0x%llx status:0x%x index:%ld  %s\n",__func__,(u64)p_file_stat_base,(u64)p_file_area,p_file_area->file_area_state,index,get_file_name_no_lock_from_mapping(mapping));
					}
				/*folio_temp等于1说明是异步内存回收线程回收的page，此时的file_area才有in_free标记，在file_stat->free链表。否则，
					 *如果page是kswapd回收的，此时folio_temp是shadow值，file_area没有in_free标记，也不在file_stat->free链表，此时不能把file_area移动到file_stat->free链表尾*/
				}else if(1 == (u64)folio_temp){
					/* write page发生refault把file_area移动到file_stat->free链表尾。file_area的每个page发生refault，都会执行一次，这个没啥好办法。
					 * 其实可以第一次设置in_refault标记，后续有in_refault标记就不再把file_area移动到file_stat->free链表尾了。但是等再次回收file-area
					 * 的page，还得再清理掉in_refault标记，弄得有点麻烦???????*/
					move_writeonly_file_area_to_free_list_tail(p_file_stat_base,p_file_area);
				}

			}
			//file_area已经添加到xarray tree，但是page还没有赋值到file_area->pages[]数组
			goto find_file_area;
			/*
			 * If a larger entry exists,
			 * it will be the first and only entry iterated.
			 */
			if (order == -1)
				order = xas_get_order(&xas);
		}

		/* entry may have changed before we re-acquire the lock */
		if (alloced_order && (old != alloced_shadow || order != alloced_order)) {
			xas_destroy(&xas);
			alloced_order = 0;
		}

		if (old) {
			if (order > 0 && order > folio_order(folio)) {
				/* How to handle large swap entries? */
				BUG_ON(shmem_mapping(mapping));
				if (!alloced_order) {
					split_order = order;
					goto unlock;
				}
				xas_split(&xas, old, order);
				xas_reset(&xas);
			}
			if (shadowp)
				*shadowp = old;
		}

		//分配file_area
		p_file_area  = file_area_alloc_and_init(area_index_for_page,p_file_stat_base,mapping);
		if(!p_file_area){
			xas_set_err(&xas, -ENOMEM);
			goto unlock; 
		}
		
		xas_store(&xas, file_area_to_entry(p_file_area));
		if (xas_error(&xas))
			goto unlock;
find_file_area:
        set_file_area_page_bit(p_file_area,page_offset_in_file_area);
		FILE_AREA_PRINT1("%s mapping:0x%llx p_file_area:0x%llx folio:0x%llx index:%ld page_offset_in_file_area:%d\n",__func__,(u64)mapping,(u64)p_file_area,(u64)folio,index,page_offset_in_file_area);
		
		/*不是NULL并且不是file_area的索引时，才触发crash，这个判断是多余的???????????????????。算了还是加上这个判断吧，多一个异常判断多点靠谱，最然看着没啥用*/
		folio_temp = rcu_dereference(p_file_area->pages[page_offset_in_file_area]);
		if(NULL != folio_temp && !folio_is_file_area_index_or_shadow(folio_temp))
		panic("%s p_file_area->pages:0x%llx != NULL error folio:0x%llx\n",__func__,(u64)p_file_area->pages[page_offset_in_file_area],(u64)folio);
				smp_wmb();

		rcu_assign_pointer(p_file_area->pages[page_offset_in_file_area], folio);
		
		mapping->nrpages += nr;

		/* hugetlb pages do not participate in page cache accounting */
		if (!huge) {
			__lruvec_stat_mod_folio(folio, NR_FILE_PAGES, nr);
			if (folio_test_pmd_mappable(folio))
				__lruvec_stat_mod_folio(folio,
						NR_FILE_THPS, nr);
		}

unlock:
		xas_unlock_irq(&xas);

		/* split needed, alloc here and retry. */
		if (split_order) {
			BUG();//不会使用大页，防护用
			xas_split_alloc(&xas, old, split_order, gfp);
			if (xas_error(&xas))
				goto error;
			alloced_shadow = old;
			alloced_order = split_order;
			xas_reset(&xas);
			continue;
		}

		if (!xas_nomem(&xas, gfp))
			break;
	}

	if (xas_error(&xas))
		goto error;

	/*trace_mm_filemap_add_to_page_cache(folio);*/
	    
	rcu_read_unlock();
	return 0;
error:

	rcu_read_unlock();

	folio->mapping = NULL;
	/* Leave page->index set: truncation relies upon it */
	folio_put_refs(folio, nr);
	return xas_error(&xas);
}
EXPORT_SYMBOL(__filemap_add_folio_for_file_area);
pgoff_t page_cache_next_miss_for_file_area(struct address_space *mapping,
		pgoff_t index, unsigned long max_scan)
{
	XA_STATE(xas, &mapping->i_pages, index >> PAGE_COUNT_IN_AREA_SHIFT);
	//令page索引与上0x3得到它在file_area的pages[]数组的下标
	unsigned int page_offset_in_file_area = index & PAGE_COUNT_IN_AREA_MASK;
	struct file_area *p_file_area;
	struct file_stat_base *p_file_stat_base;
	unsigned long folio_index_from_xa_index = 0;
	struct folio *folio;
    
	/*该函数没有rcu_read_lock，但是调用者里已经执行了rcu_read_lock，这点需要注意!!!!!!!!!!!!!!*/

	p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;
	smp_rmb();
	if(unlikely(!IS_SUPPORT_FILE_AREA_READ_WRITE(mapping)))
        printk("%s %s %d mapping:0x%llx file_stat:0x%lx has delete,do not use this file_stat!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",__func__,current->comm,current->pid,(u64)mapping,mapping->rh_reserved1);

	/*while (max_scan--) {//max_scan原本代表扫描的最多扫描的page数，现在代表的是最多扫描的file_area数，
	 *自然不能再用了。于是放到下边if(max_scan)那里*/
	while (1) {
		//xas_next()里边自动令xas->xa_index和xas->xa_offset加1
		void *entry = xas_next(&xas);
		//folio_index_from_xa_index = (xas.xa_index << PAGE_COUNT_IN_AREA_SHIFT) + page_offset_in_file_area;不能放到这里，否则下边find_page_from_file_area循环查找page时，folio_index_from_xa_index无法得到更新，始终是file_area的第一个folio的索引
		if(!entry)
			return ((xas.xa_index << PAGE_COUNT_IN_AREA_SHIFT) + page_offset_in_file_area);

		if(xa_is_value(entry) || !is_file_area_entry(entry))
			panic("%s mapping:0x%llx p_file_area:0x%llx error\n",__func__,(u64)mapping,(u64)entry);

		p_file_area = entry_to_file_area(entry);
find_page_from_file_area:
		max_scan--;
		if(0 == max_scan)
			break;

		folio_index_from_xa_index = (xas.xa_index << PAGE_COUNT_IN_AREA_SHIFT) + page_offset_in_file_area;
		if (folio_index_from_xa_index  == 0)
			return 0;
		
		folio = rcu_dereference(p_file_area->pages[page_offset_in_file_area]);
		folio_is_file_area_index_or_shadow_and_clear_NULL(folio);
		if(!folio)
			return folio_index_from_xa_index;

		/* 如果有进程此时并发page_cache_delete_for_file_area()里释放该page，这个内存屏障，确保，看到的page不是NULL时，
		 * page在file_area->file_area_statue的对应的bit位一定是1，不是0*/
		smp_rmb();
		/*检测查找到的page是否正确，不是则crash*/
		CHECK_FOLIO_FROM_FILE_AREA_VALID(&xas,mapping,p_file_area->pages[page_offset_in_file_area],p_file_area,page_offset_in_file_area,folio_index_from_xa_index);

		page_offset_in_file_area ++;

		/*如果file_area里还有page没遍历到，goto find_page_from_file_area去查找file_area里的下一个page。否则到while循环开头
		 *xas_next(&xas)去查找下一个file_area，此时需要find_page_from_file_area清0，这个很关键*/
		if(page_offset_in_file_area < PAGE_COUNT_IN_AREA){
			/*page_offset_in_file_area加1不能放到这里，重大逻辑错误。比如，上边判断page_offset_in_file_area是3的folio，
			 *然后执行到f(page_offset_in_file_area < PAGE_COUNT_IN_AREA)判断时，正常就应该不成立的，因为file_area的最后一个folio已经遍历过了*/
			//page_offset_in_file_area ++;
			goto find_page_from_file_area;
		}
		else
			page_offset_in_file_area = 0;
	}

	FILE_AREA_PRINT("%s mapping:0x%llx index:%ld return:%ld\n",__func__,(u64)mapping,index,xas.xa_index + page_offset_in_file_area);
	
    return index + max_scan;
}
EXPORT_SYMBOL(page_cache_next_miss_for_file_area);
pgoff_t page_cache_prev_miss_for_file_area(struct address_space *mapping,
		pgoff_t index, unsigned long max_scan)
{
	XA_STATE(xas, &mapping->i_pages, index >> PAGE_COUNT_IN_AREA_SHIFT);
	//令page索引与上0x3得到它在file_area的pages[]数组的下标
	unsigned int page_offset_in_file_area = index & PAGE_COUNT_IN_AREA_MASK;
	struct file_area *p_file_area;
	struct file_stat_base *p_file_stat_base;
	unsigned long folio_index_from_xa_index = 0 ;
	struct folio *folio;

	p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;
	smp_rmb();
	if(unlikely(!IS_SUPPORT_FILE_AREA_READ_WRITE(mapping)))
        printk("%s %s %d mapping:0x%llx file_stat:0x%lx has delete,do not use this file_stat!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",__func__,current->comm,current->pid,(u64)mapping,mapping->rh_reserved1);

	/*while (max_scan--) {//max_scan原本代表扫描的最多扫描的page数，现在代表的是最多扫描的file_area数，
	 *自然不能再用了。于是放到下边if(max_scan)那里*/
	while (1) {
		//xas_prev()里边自动令xas->xa_index和xas->xa_offset减1
		void *entry = xas_prev(&xas);
		if (!entry)
			break;
		if(xa_is_value(entry) || !is_file_area_entry(entry))
			panic("%s mapping:0x%llx p_file_area:0x%llx error\n",__func__,(u64)mapping,(u64)entry);

		p_file_area = entry_to_file_area(entry);
find_page_from_file_area:
		max_scan--;
		if(0 == max_scan)
			break;

		folio_index_from_xa_index = (xas.xa_index << PAGE_COUNT_IN_AREA_SHIFT) + page_offset_in_file_area;
		if (folio_index_from_xa_index == ULONG_MAX)
			break;

		folio = rcu_dereference(p_file_area->pages[page_offset_in_file_area]);
		/*如果folio是file_area的索引，则对folio清NULL，避免folio干扰后续判断*/
		folio_is_file_area_index_or_shadow_and_clear_NULL(folio);
		//page是NULL则直接break，这个跟page_cache_prev_miss函数原有的if (!entry)break 同理，即遇到第一个NULL page则break结束查找
		if(!folio)
			break;
		
		/* 如果有进程此时并发page_cache_delete_for_file_area()里释放该page，这个内存屏障，确保，看到的page不是NULL时，
		 * page在file_area->file_area_statue的对应的bit位一定是1，不是0*/
		smp_rmb();
		/*检测查找到的page是否正确，不是则crash*/
		CHECK_FOLIO_FROM_FILE_AREA_VALID(&xas,mapping,p_file_area->pages[page_offset_in_file_area],p_file_area,page_offset_in_file_area,folio_index_from_xa_index);

		/*如果page_offset_in_file_area是0,则说明file_area的page都被遍历过了，那就到for循环开头xas_prev(&xas)去查找上一个file_area。
		 *否则，只是令page_offset_in_file_area减1，goto find_page_from_file_area去查找file_area里的上一个page*/
		if(page_offset_in_file_area == 0)
			page_offset_in_file_area = PAGE_COUNT_IN_AREA - 1;
		else{
			page_offset_in_file_area --;
			goto find_page_from_file_area;
		}
	}

	FILE_AREA_PRINT("%s mapping:0x%llx index:%ld return:%ld\n",__func__,(u64)mapping,index,xas.xa_index + page_offset_in_file_area);
	
	return ((xas.xa_index << PAGE_COUNT_IN_AREA_SHIFT) + page_offset_in_file_area);
}
EXPORT_SYMBOL(page_cache_prev_miss_for_file_area);
void *filemap_get_entry_for_file_area(struct address_space *mapping, pgoff_t index)
{
	//page索引除以2，转成file_area索引
	unsigned int area_index_for_page = index >> PAGE_COUNT_IN_AREA_SHIFT;
	XA_STATE(xas, &mapping->i_pages, area_index_for_page);
	struct folio *folio = NULL;

	struct file_stat_base *p_file_stat_base;
	struct file_area *p_file_area;
	//令page索引与上0x3得到它在file_area的pages[]数组的下标
	unsigned int page_offset_in_file_area = index & PAGE_COUNT_IN_AREA_MASK;
	rcu_read_lock();

	p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;
	smp_rmb();

    /*执行到这里，可能mapping->rh_reserved1指向的file_stat被释放了，该文件的文件页page都被释放了。用不用这里直接return NULL，不再执行下边的
	 * p_file_area = xas_load(&xas)遍历xarray tree？怕此时遍历xarray tree有问题!没事，因为此时xarray tree是空树，p_file_area = xas_load(&xas)
	 * 直接返回NULL，和直接return NULL一样的效果*/

repeat:
	xas_reset(&xas);

	p_file_area = xas_load(&xas);

	/*之前得做if (xas_retry(&xas, folio))等3个if判断，现在只用做if(!is_file_area_entry(p_file_area))判断就行了*/
	if(!is_file_area_entry(p_file_area)){
		if(!p_file_area)
			goto out;

		/*xas_retry()里有xas->xa_node = XAS_RESTART，这个隐藏的很深，这样执行xas_next(&xas)时，if(xas_not_node(node))成立，直接从
		 *xarray tree按照老的xas->xa_index重新查找，不会再执行xas->xa_index++和xas->xa_offset++而从父节点直接获取下一个索引的成员了*/
		if (xas_retry(&xas, p_file_area))
			goto repeat;

		panic("%s mapping:0x%llx p_file_area:0x%llx error!!!!!!!!!!!!!!!!!!!!!!!!\n",__func__,(u64)mapping,(u64)p_file_area);
		if(xa_is_value(p_file_area))
			goto out;
	}

	p_file_area = entry_to_file_area(p_file_area);
	folio = rcu_dereference(p_file_area->pages[page_offset_in_file_area]);
	/*如果folio是file_area的索引，则对folio清NULL，避免folio干扰后续判断*/
	folio_is_file_area_index_or_shadow_and_clear_NULL(folio);

	if (!folio /*|| xa_is_value(folio)*/)//xa_is_value()只是看bit0是否是1，其他bit位不用管
		goto out;


	/* 检测查找到的page是否正确，不是则crash。由于最新版本，还会判断查找到的page对应的file_area->file_area_state的
	 * bit位是否置1了，表示该page保存到了file_area->pages[]数组，没有置1就要crash。但是有个并发问题，如果
	 * 该page此时被其他进程执行page_cache_delete()并发删除，会并发把page在file_area->file_area_statue的对应的bit位
	 * 清0，导致这里判定page存在但是page在file_area->file_area_statue的对应的bit位缺时0，于是会触发crash。解决
	 * 方法时，把这个判断放到该page判定有效且没有被其他进程并发释放后边*/
	//CHECK_FOLIO_FROM_FILE_AREA_VALID(&xas,folio,p_file_area,page_offset_in_file_area,((xas.xa_index << PAGE_COUNT_IN_AREA_SHIFT) + page_offset_in_file_area));

	if (!folio_try_get_rcu(folio))
		goto repeat;

	if (unlikely(folio != rcu_dereference(p_file_area->pages[page_offset_in_file_area]))) {
		folio_put(folio);
		goto repeat;
	}
	/*到这里才判定page有有效，没有被其他进程并发释放掉*/
	CHECK_FOLIO_FROM_FILE_AREA_VALID(&xas,mapping,folio,p_file_area,page_offset_in_file_area,((xas.xa_index << PAGE_COUNT_IN_AREA_SHIFT) + page_offset_in_file_area));
	//统计page引用计数
	hot_file_update_file_status(mapping,p_file_stat_base,p_file_area,FILE_AREA_PAGE_IS_WRITE);


out:
	rcu_read_unlock();

	FILE_AREA_PRINT("%s mapping:0x%llx p_file_area:0x%llx folio:0x%llx index:%ld\n",__func__,(u64)mapping,(u64)p_file_area,(u64)folio,index);

	return folio;
}
EXPORT_SYMBOL(filemap_get_entry_for_file_area);
/*这个函数可以加入node cache机制。这个函数做成inline形式，因为调用频繁，降低性能损耗*/
void *get_folio_from_file_area_for_file_area(struct address_space *mapping,pgoff_t index)
{
	struct file_area *p_file_area;
	struct file_stat_base *p_file_stat_base;
	struct folio *folio = NULL;

	rcu_read_lock();
	p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;
	smp_rmb();

	/*内存屏障后再探测mapping->rh_reserved1是否是0，即对应文件inode已经被释放了。那mapping已经失效*/
	if(IS_SUPPORT_FILE_AREA_READ_WRITE(mapping)){
		p_file_area = xa_load(&mapping->i_pages,index >> PAGE_COUNT_IN_AREA_SHIFT);
		if(!p_file_area)
		{
			goto out;
		}
		p_file_area = entry_to_file_area(p_file_area);
		folio = p_file_area->pages[index & PAGE_COUNT_IN_AREA_MASK];
		/*如果folio是file_area的索引，则对folio清NULL，避免folio干扰后续判断*/
		folio_is_file_area_index_or_shadow_and_clear_NULL(folio);
	
		/* 到这里才判定page有有效，没有被其他进程并发释放掉。但这里是内核预读代码page_cache_ra_unbounded()调用的，
		 * 原生代码并没有判定该page是否会因page内存回收而判定page是否无效，这里还要判断吗？目前只判断索引*/
		if(folio && (/*folio->index != index ||*/ folio->mapping != mapping)){
			/*if成立说明该folio被内存回收了，那就设置folio为NULL而无效。这里确实遇到过，是正常现象，不能panic*/
	        //panic("%s %s %d index:%ld folio->index:%ld folio:0x%llx mapping:0x%llx\n",__func__,current->comm,current->pid,index,folio->index,(u64)folio,(u64)mapping);
	        pr_warn("%s %s %d index:%ld folio->index:%ld folio:0x%llx mapping:0x%llx\n",__func__,current->comm,current->pid,index,folio->index,(u64)folio,(u64)mapping);
			folio = NULL;
		}
	}
out:
	rcu_read_unlock();
	FILE_AREA_PRINT("%s mapping:0x%llx p_file_area:0x%llx folio:0x%llx index:%ld\n",__func__,(u64)mapping,(u64)p_file_area,(u64)folio,index);

	if(folio && !file_area_in_read(p_file_area)){
		set_file_area_in_read(p_file_area);
		if(file_stat_in_writeonly_base(p_file_stat_base))
			clear_file_stat_in_writeonly_base(p_file_stat_base);

	}
	return (void *)folio;
}
EXPORT_SYMBOL(get_folio_from_file_area_for_file_area);

/*find_get_entry函数原本目录是：以xas->xa_index为起始索引，去xarray tree查找第一个不是NULL的page，
 *如果找到索引大于max，直接返回NULL。因此，这个函数这样设计，先xas_find()查找file_area，这个file_area可能
 *没有一个page，那就goto retry继续查找，直到xas_find(xas, max)因查到的索引大于max返回NULL，该函数直接return。
 *如果xas_find()找到的file_area有page，则判断合法后直接return folio。然后下次你再执行改函数时，就不能直接执行
 *xas_find(xas, max)了，而要去刚才查到的page的file_area里，继续查找其他page。直到一个file_area的page全被获取到
 *才能执行xas_find(xas, max)去查找下一个file_area*/
inline struct folio *find_get_entry_for_file_area(struct xa_state *xas, pgoff_t max,
		xa_mark_t mark,struct file_area **p_file_area,unsigned int *page_offset_in_file_area,struct address_space *mapping)
{
	struct folio *folio;
	//计算要查找的最大page索引对应的file_area索引
	pgoff_t file_area_max = max >> PAGE_COUNT_IN_AREA_SHIFT;
	unsigned long folio_index_from_xa_index;
	void *old_entry;

	/*如果*p_file_area不是NULL，说明上次执行该函数里的xas_find(xas, max)找到的file_area，还有剩余的page没有获取
	 *先goto find_page_from_file_area分支把这个file_area剩下的page探测完*/
	if(*p_file_area != NULL)
		goto find_page_from_file_area;
retry:
	/*这里会令xas.xa_index和xas.xa_offset自动加1，然后去查找下一个file_area。那自然是不行的。find_get_entries()里
	 *folio = find_get_entry_for_file_area(&xas, end, XA_PRESENT)调用该函数，只是一次获取一个page，不是一个file_area。
	 *要保证一个file_area的page全被获取过了，才能再执行xas_find()获取下一个file_area*/
	if (mark == XA_PRESENT)
		*p_file_area = xas_find(xas, file_area_max);/*这里的必须是查找的最大file_area的索引file_area_max，不能最大page索引max*/
	else
		*p_file_area = xas_find_marked(xas, file_area_max, mark);

	if (NULL == *p_file_area){
		FILE_AREA_PRINT("%s %s %d p_file_area NULL max:%ld xas.xa_index:%ld page_offset_in_file_area:%d\n",__func__,current->comm,current->pid,max,xas->xa_index,*page_offset_in_file_area);

		return NULL;
	}

	if (xas_retry(xas, *p_file_area))
		goto retry;

    /*
	 * A shadow entry of a recently evicted page, a swap
	 * entry from shmem/tmpfs or a DAX entry.  Return it
	 * without attempting to raise page count.
	 */
	*p_file_area = entry_to_file_area(*p_file_area);

	if((*p_file_area)->mapping && !file_area_have_page(*p_file_area) && mapping_exiting(mapping)){
		/*为了不干扰原有的xas，重新定义一个xas_del*/
		//XA_STATE(xas_del, &mapping->i_pages, (*p_file_area)->start_index >> PAGE_COUNT_IN_AREA_SHIFT);
		XA_STATE(xas_del, &mapping->i_pages, (*p_file_area)->start_index);
		
		WRITE_ONCE((*p_file_area)->mapping, 0);
		smp_wmb();
		/*需要用文件xarray tree的lock加锁，因为xas_store()操作必须要xarray tree加锁*/
		xas_lock_irq(&xas_del);
		/*正常情况这里不可能成立，因为此时文件inode处于iput()，不会有进程访问产生新的page。
		  况且上边已经做了if(!file_area_have_page(*p_file_area))防护，以防万一还是spin_lock
		  加锁再防护一次，防止此时有进程并发读写该文件导致file_area分配了新的page*/
		if(file_area_have_page(*p_file_area))
			panic("%s file_area:0x%llx have page\n",__func__,(u64)(*p_file_area));
		old_entry = xas_store(&xas_del, NULL);
		xas_unlock_irq(&xas_del);

		if(/*old_entry && */file_stat_in_global_base((struct file_stat_base *)mapping->rh_reserved1)){
			if(old_entry){
				/*注意，set_file_area_in_mapping_delete必须放到这里做，确保该file_area没有因为长时间没访问，被判定是冷file_area，
				 *而被异步内存回收线程主动执行cold_file_area_delete()释放掉*/
				set_file_area_in_mapping_delete(*p_file_area);
			}else{
				printk("%s file_area:0x%llx old_entry NULL\n",__func__,(u64)p_file_area);
			}
				
		}else{
		}

		*page_offset_in_file_area = 0;
		/*goto retry分支里执行xas_find()，会自动令xas->xa_offset++，进而查找下一个索引的file_area*/
		goto retry;
	}

find_page_from_file_area:
	if(*page_offset_in_file_area >= PAGE_COUNT_IN_AREA){
		panic("%s p_file_area:0x%llx page_offset_in_file_area:%d error\n",__func__,(u64)*p_file_area,*page_offset_in_file_area);
	}

	folio_index_from_xa_index = (xas->xa_index << PAGE_COUNT_IN_AREA_SHIFT) + *page_offset_in_file_area;
	if(folio_index_from_xa_index > max){
		FILE_AREA_PRINT("%s %s %d p_file_area:0x%llx max:%ld xas.xa_index:%ld page_offset_in_file_area:%d return NULL\n",__func__,current->comm,current->pid,(u64)*p_file_area,max,xas->xa_index,*page_offset_in_file_area);

		return NULL;
	}

	folio = rcu_dereference((*p_file_area)->pages[*page_offset_in_file_area]);
	/*如果fiolio是file_area的索引，则对folio清NULL，避免folio干扰后续判断*/
	folio_is_file_area_index_or_shadow_and_clear_NULL(folio);
	FILE_AREA_PRINT("%s %s %d p_file_area:0x%llx file_area_state:0x%x folio:0x%llx xas.xa_index:%ld folio->index:%ld\n",__func__,current->comm,current->pid,(u64)*p_file_area,(*p_file_area)->file_area_state,(u64)folio,xas->xa_index,folio != NULL ?folio->index:-1);

	if(!folio)
		goto next_folio;

	if (!folio_try_get_rcu(folio))
		goto reset;

	if (unlikely(folio != rcu_dereference((*p_file_area)->pages[*page_offset_in_file_area]))) {
		folio_put(folio);
		goto reset;
	}
	CHECK_FOLIO_FROM_FILE_AREA_VALID(xas,mapping,folio,*p_file_area,*page_offset_in_file_area,folio_index_from_xa_index);

next_folio:
	*page_offset_in_file_area = *page_offset_in_file_area + 1;
	/*如果page_offset_in_file_area是4，说明一个file_area里的page都被获取过了，如果下次再执行该函数，必须执行xas_find(xas, max)
	 *获取新的file_area。于是令*p_file_area=NULL, *page_offset_in_file_area清0。这样下次执行该函数，才会执行xas_find(xas, max)
	 *查找下一个file_area，并从这个file_area的第一个page开始*/
	if(*page_offset_in_file_area == PAGE_COUNT_IN_AREA){
		*p_file_area = NULL;
		*page_offset_in_file_area = 0;
		/*如果此时folio是NULL，不能在下边return folio返回NULL，这会结束page查找。而要goto folio分支，去执行
		 *p_file_area = xas_find(xas, file_area_max)去查找下一个file_area，然后获取这个新的file_area的page。这个函数能直接return的只有3种情况
		 *1:查找的file_area索引大于file_area_max(if(!*p_file_area)那里) 2：查找到的page索引大于大于max(if(folio->index > max)那里) 3：查找到了有效page(即return folio)*/
		if(!folio)
			goto retry;
	}
	else{
		/*去查找当前file_area的下一个page。但只有folio是NULL的情况下!!!!!!!如果folio是合法的，直接return folio返回给调用者。
		 *然后调用者下次执行该函数，因为 *p_file_area 不是NULL，直接获取这个file_area的下一个page*/
		if(!folio)
			goto find_page_from_file_area;
	}
	return folio;
reset:
	/*xas_reset()会xas->xa_node = XAS_RESTART，然后goto retry时执行xas_find()时，直接执行entry = xas_load(xas)重新获取当前索引的file_area。
	 *如果xas->xa_node不是XAS_RESTART，那xas_find()里是先执行xas_next_offset(xas)令xas->xa_offset加1、xas->xa_index加1，然后去查询
	 *下一个索引file_area。简单说，一个是重新查询当前索引的file_area，一个是查询下一个索引的file_area*/
	xas_reset(xas);
	goto retry;
}
EXPORT_SYMBOL(find_get_entry_for_file_area);
unsigned find_get_entries_for_file_area(struct address_space *mapping, pgoff_t *start,
		pgoff_t end, struct folio_batch *fbatch, pgoff_t *indices)
{
	XA_STATE(xas, &mapping->i_pages, *start >> PAGE_COUNT_IN_AREA_SHIFT);
	struct folio *folio;
	//令page索引与上0x3得到它在file_area的pages[]数组的下标
	unsigned int page_offset_in_file_area = *start & PAGE_COUNT_IN_AREA_MASK;
	/*必须赋初值NULL，表示file_area无效，这样find_get_entry_for_file_area()里才会xas_find()查找*/
	struct file_area *p_file_area = NULL;
	struct file_stat_base *p_file_stat_base;

	FILE_AREA_PRINT("%s %s %d mapping:0x%llx start:%ld end:%ld\n",__func__,current->comm,current->pid,(u64)mapping,start,end);
	
	rcu_read_lock();
	p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;
	smp_rmb();
	if(unlikely(!IS_SUPPORT_FILE_AREA_READ_WRITE(mapping)))
        printk("%s %s %d mapping:0x%llx file_stat:0x%lx has delete,do not use this file_stat!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",__func__,current->comm,current->pid,(u64)mapping,mapping->rh_reserved1);

	while ((folio = find_get_entry_for_file_area(&xas, end, XA_PRESENT,&p_file_area,&page_offset_in_file_area,mapping)) != NULL) {
		indices[fbatch->nr] = folio->index;
		if (!folio_batch_add(fbatch, folio))
			break;
	}
	
	if (folio_batch_count(fbatch)) {
		unsigned long nr;
		int idx = folio_batch_count(fbatch) - 1;

		folio = fbatch->folios[idx];
		if (!xa_is_value(folio))
			nr = folio_nr_pages(folio);
		else
			BUG();

		*start = round_down(indices[idx] + nr, nr);
	}
	rcu_read_unlock();

	return folio_batch_count(fbatch);
}
EXPORT_SYMBOL(find_get_entries_for_file_area);
unsigned find_lock_entries_for_file_area(struct address_space *mapping, pgoff_t *start,
		pgoff_t end, struct folio_batch *fbatch, pgoff_t *indices)
{
	XA_STATE(xas, &mapping->i_pages, *start >> PAGE_COUNT_IN_AREA_SHIFT);
	struct folio *folio;
	//令page索引与上0x3得到它在file_area的pages[]数组的下标
	unsigned int page_offset_in_file_area = *start & PAGE_COUNT_IN_AREA_MASK;
	/*必须赋初值NULL，表示file_area无效，这样find_get_entry_for_file_area()里才会xas_find()查找*/
	struct file_area *p_file_area = NULL;
	struct file_stat_base *p_file_stat_base;

	FILE_AREA_PRINT("%s %s %d mapping:0x%llx start:%ld end:%ld\n",__func__,current->comm,current->pid,(u64)mapping,start,end);
	
	rcu_read_lock();
	p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;
	smp_rmb();
	if(unlikely(!IS_SUPPORT_FILE_AREA_READ_WRITE(mapping)))
        printk("%s %s %d mapping:0x%llx file_stat:0x%lx has delete,do not use this file_stat!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",__func__,current->comm,current->pid,(u64)mapping,mapping->rh_reserved1);

	while ((folio = find_get_entry_for_file_area(&xas, end, XA_PRESENT,&p_file_area,&page_offset_in_file_area,mapping))) {
		unsigned long base;
		unsigned long nr;
		
		if (!xa_is_value(folio)) {
			nr = folio_nr_pages(folio);
			base = folio->index;
			/* Omit large folio which begins before the start */
			if (base < *start)
				goto put;
			/* Omit large folio which extends beyond the end */
			if (base + nr - 1 > end)
				goto put;
			if (!folio_trylock(folio))
				goto put;
			if (folio->mapping != mapping ||
					folio_test_writeback(folio))
				goto unlock;
			VM_BUG_ON_FOLIO(!folio_contains(folio, folio->index),
					folio);
		}else{
			nr = 1 << xas_get_order(&xas);
			base = xas.xa_index & ~(nr - 1);
			/* Omit order>0 value which begins before the start */
			if (base < *start)
				continue;
			/* Omit order>0 value which extends beyond the end */
			if (base + nr - 1 > end)
				break;
		}
		/* Update start now so that last update is correct on return */
		*start = base + nr;
		indices[fbatch->nr] = ((xas.xa_index << PAGE_COUNT_IN_AREA_SHIFT) + page_offset_in_file_area);
		if (!folio_batch_add(fbatch, folio))
			break;
		continue;
unlock:
		folio_unlock(folio);
put:
		folio_put(folio);
	}
	rcu_read_unlock();

	return folio_batch_count(fbatch);
}
EXPORT_SYMBOL(find_lock_entries_for_file_area);
unsigned find_get_pages_range_for_file_area(struct address_space *mapping, pgoff_t *start,
		pgoff_t end, unsigned int nr_pages,
		struct page **pages)
{
	XA_STATE(xas, &mapping->i_pages, *start >> PAGE_COUNT_IN_AREA_SHIFT);
	struct folio *folio;
	unsigned ret = 0;
	//令page索引与上0x3得到它在file_area的pages[]数组的下标
	unsigned int page_offset_in_file_area = *start & PAGE_COUNT_IN_AREA_MASK;
	/*必须赋初值NULL，表示file_area无效，这样find_get_entry_for_file_area()里才会xas_find()查找*/
	struct file_area *p_file_area = NULL;
	struct file_stat_base *p_file_stat_base;

	if (unlikely(!nr_pages))
		return 0;

	FILE_AREA_PRINT("%s %s %d mapping:0x%llx start:%ld end:%ld nr_pages:%d\n",__func__,current->comm,current->pid,(u64)mapping,*start,end,nr_pages);

	rcu_read_lock();
	p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;
	smp_rmb();
	if(unlikely(!IS_SUPPORT_FILE_AREA_READ_WRITE(mapping)))
        printk("%s %s %d mapping:0x%llx file_stat:0x%lx has delete,do not use this file_stat!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",__func__,current->comm,current->pid,(u64)mapping,mapping->rh_reserved1);

	while ((folio = find_get_entry_for_file_area(&xas, end, XA_PRESENT,&p_file_area,&page_offset_in_file_area,mapping))) {
		/* Skip over shadow, swap and DAX entries */
		if (xa_is_value(folio))
			continue;

		if(folio_nr_pages(folio) > 1)
			panic("%s folio:0x%llx folio_nr_pages > 1 %ld\n",__func__,(u64)folio,folio_nr_pages(folio));

		pages[ret] = folio_file_page(folio, folio->index);
		if (++ret == nr_pages) {
			*start = folio->index + 1;
			goto out;
		}
	}

	/*
	 * We come here when there is no page beyond @end. We take care to not
	 * overflow the index @start as it confuses some of the callers. This
	 * breaks the iteration when there is a page at index -1 but that is
	 * already broken anyway.
	 */
	if (end == (pgoff_t)-1)
		*start = (pgoff_t)-1;
	else
		*start = end + 1;
out:
	rcu_read_unlock();

	return ret;
}
EXPORT_SYMBOL(find_get_pages_range_for_file_area);
#define  folio_index_for_file_area(xas,page_offset_in_file_area) ((xas.xa_index << PAGE_COUNT_IN_AREA_SHIFT) + page_offset_in_file_area)
unsigned filemap_get_folios_contig_for_file_area(struct address_space *mapping,
		pgoff_t *start, pgoff_t end, struct folio_batch *fbatch)
{
	XA_STATE(xas, &mapping->i_pages, *start >> PAGE_COUNT_IN_AREA_SHIFT);
	unsigned long nr;
	struct folio *folio;
	unsigned int ret = 0;

	struct file_area *p_file_area;
	struct file_stat_base *p_file_stat_base;
	//令page索引与上0x3得到它在file_area的pages[]数组的下标
	unsigned int page_offset_in_file_area = *start & PAGE_COUNT_IN_AREA_MASK;


	FILE_AREA_PRINT("%s mapping:0x%llx index:%ld nr_pages:%d\n",__func__,(u64)mapping,index,nr_pages);

	rcu_read_lock();
	p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;
	smp_rmb();
	if(unlikely(!IS_SUPPORT_FILE_AREA_READ_WRITE(mapping)))
        printk("%s %s %d mapping:0x%llx file_stat:0x%lx has delete,do not use this file_stat!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",__func__,current->comm,current->pid,(u64)mapping,mapping->rh_reserved1);

	for (p_file_area = xas_load(&xas); p_file_area; p_file_area = xas_next(&xas)) {

		/*xas_retry()里有xas->xa_node = XAS_RESTART，这个隐藏的很深，这样执行xas_next(&xas)时，if(xas_not_node(node))成立，直接从
		 *xarray tree按照老的xas->xa_index重新查找，不会再执行xas->xa_index++和xas->xa_offset++而从父节点直接取出成员了*/
		if (xas_retry(&xas, p_file_area))
			continue;

		if(xa_is_value(p_file_area) || xa_is_sibling(p_file_area))
			panic("%s p_file_area:0x%llx error\n",__func__,(u64)p_file_area);

		p_file_area = entry_to_file_area(p_file_area);

find_page_from_file_area:
		folio = rcu_dereference(p_file_area->pages[page_offset_in_file_area]);
		/*如果fiolio是file_area的索引，则对folio清NULL，避免folio干扰后续判断*/
		folio_is_file_area_index_or_shadow_and_clear_NULL(folio);
		/*这个跟filemap_get_read_batch()里for (folio = xas_load(&xas); folio; folio = xas_next(&xas))判断出folio是NULL则结束循环是一个效果*/
		if(!folio || folio_index_for_file_area(xas,page_offset_in_file_area) > end)
			break;


		/*如果获取的page引用计数是0，说明已经被其他进程释放了。则直接goto retry从xarray tree按照老的xas.xa_index重新查找
		 *file_area，然后查找page。其实没有必要重新查找file_area，直接goto find_page_from_file_area重新获取page就行了!!!!!!!!!!*/
		if (!folio_try_get_rcu(folio))
			goto retry;

		if (unlikely(folio != rcu_dereference(p_file_area->pages[page_offset_in_file_area]))){ 
			/*当前page获取失败，把folio_put(folio)释放引用计数放到这里，然后goto next_folio分支，直接获取下一个page。回归*/
			goto put_folio;
		}
		CHECK_FOLIO_FROM_FILE_AREA_VALID(&xas,mapping,folio,p_file_area,page_offset_in_file_area,((xas.xa_index << PAGE_COUNT_IN_AREA_SHIFT) + page_offset_in_file_area));

		if (!folio_batch_add(fbatch, folio)) {
			nr = folio_nr_pages(folio);
			*start = folio->index + nr;
			goto out;
		}
		if(folio_nr_pages(folio) > 1){
			panic("%s folio:0x%llx folio_nr_pages:%ld\n",__func__,(u64)folio,folio_nr_pages(folio));
		}
next_folio:
		page_offset_in_file_area ++;

		/*如果file_area里还有page没遍历到，goto find_page_from_file_area去查找file_area里的下一个page。否则到for循环开头
		 *xas_for_each()去查找下一个file_area，此时需要find_page_from_file_area清0，这个很关键*/
		if(page_offset_in_file_area < PAGE_COUNT_IN_AREA){
			/*page_offset_in_file_area加1不能放到这里，重大逻辑错误。比如，上边判断page_offset_in_file_area是3的folio，
			 *然后执行到f(page_offset_in_file_area < PAGE_COUNT_IN_AREA)判断时，正常就应该不成立的，因为file_area的最后一个folio已经遍历过了*/
			goto find_page_from_file_area;
		}
		else{
			//要查找下一个file_area了，page_offset_in_file_area要清0
			page_offset_in_file_area = 0;
		}

		continue;
put_folio:/*这段代码移动到上边了，我的思路错了。上边if(unlikely(folio != xas_reload(&xas)))成立，需要重新从xarray tree重新读取同一个索引的page，我却错误以为要读取下一个索引的page*/
		folio_put(folio);
retry:
		xas_reset(&xas);
	}
	
update_start:
	nr = folio_batch_count(fbatch);

	if (nr) {
		folio = fbatch->folios[nr - 1];
		*start = folio_next_index(folio);
	}
out:	
	rcu_read_unlock();
	return folio_batch_count(fbatch);
}
EXPORT_SYMBOL(filemap_get_folios_contig_for_file_area);
unsigned filemap_get_folios_tag_for_file_area(struct address_space *mapping, pgoff_t *start,
			pgoff_t end, xa_mark_t tag, struct folio_batch *fbatch)
{
	XA_STATE(xas, &mapping->i_pages, *start >> PAGE_COUNT_IN_AREA_SHIFT);
	struct folio *folio;
	unsigned ret = 0;
	//令page索引与上0x3得到它在file_area的pages[]数组的下标
	unsigned int page_offset_in_file_area = *start & PAGE_COUNT_IN_AREA_MASK;
	/*必须赋初值NULL，表示file_area无效，这样find_get_entry_for_file_area()里才会xas_find()查找*/
	struct file_area *p_file_area = NULL;
	struct file_stat_base *p_file_stat_base;


	FILE_AREA_PRINT("%s %s %d mapping:0x%llx index:%ld nr_pages:%d end:%ld tag:%d page_offset_in_file_area:%d xas.xa_index:%ld\n",__func__,current->comm,current->pid,(u64)mapping,*index,nr_pages,end,tag,page_offset_in_file_area,xas.xa_index);

	rcu_read_lock();
	p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;
	smp_rmb();
	if(unlikely(!IS_SUPPORT_FILE_AREA_READ_WRITE(mapping)))
        printk("%s %s %d mapping:0x%llx file_stat:0x%lx has delete,do not use this file_stat!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",__func__,current->comm,current->pid,(u64)mapping,mapping->rh_reserved1);

	while ((folio = find_get_entry_for_file_area(&xas, end, tag,&p_file_area,&page_offset_in_file_area,mapping)) != NULL) {
		
		/*
		 * Shadow entries should never be tagged, but this iteration
		 * is lockless so there is a window for page reclaim to evict
		 * a page we saw tagged.  Skip over it.
		 */
		if (xa_is_value(folio))
			continue;

		if(folio_nr_pages(folio) > 1)
			panic("%s folio:0x%llx folio_nr_pages > 1 %ld\n",__func__,(u64)folio,folio_nr_pages(folio));

		if (!folio_batch_add(fbatch, folio)) {
			unsigned long nr = folio_nr_pages(folio);
			*start = folio->index + nr;
			goto out;
		}
	}

	/*
	 * We come here when there is no page beyond @end. We take care to not
	 * overflow the index @start as it confuses some of the callers. This
	 * breaks the iteration when there is a page at index -1 but that is
	 * already broke anyway.
	 */
	if (end == (pgoff_t)-1)
		*start = (pgoff_t)-1;
	else
		*start = end + 1;
out:
	rcu_read_unlock();

	return folio_batch_count(fbatch);;
}
EXPORT_SYMBOL(filemap_get_folios_tag_for_file_area);
void filemap_get_read_batch_for_file_area(struct address_space *mapping,
		pgoff_t index, pgoff_t max, struct folio_batch *fbatch)
{
	XA_STATE(xas, &mapping->i_pages, index>>PAGE_COUNT_IN_AREA_SHIFT);
	struct folio *folio = NULL;
    
	//令page索引与上0x3得到它在file_area的pages[]数组的下标
	unsigned int page_offset_in_file_area = index & PAGE_COUNT_IN_AREA_MASK;
	struct file_stat_base *p_file_stat_base;
	struct file_area *p_file_area = NULL;
	unsigned long folio_index_from_xa_index;
	
	rcu_read_lock();
	p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;
	smp_rmb();

	for (p_file_area = xas_load(&xas); p_file_area; p_file_area = xas_next(&xas)) {

		/*之前得做if (xas_retry(&xas, folio))等3个if判断，现在只用做if(!is_file_area_entry(p_file_area))判断就行了。到这里
		 *的p_file_area一定不是NULL，不用做这个防护*/
		if(!is_file_area_entry(p_file_area)){

		    /*xas_retry()里有xas->xa_node = XAS_RESTART，这个隐藏的很深，这样执行xas_next(&xas)时，if(xas_not_node(node))成立，直接从
		     *xarray tree按照老的xas->xa_index重新查找，不会再执行xas->xa_index++和xas->xa_offset++而从父节点直接获取下一个索引的成员了*/
		    if (xas_retry(&xas, p_file_area))
			    continue;
			
			panic("%s mapping:0x%llx p_file_area:0x%llx error!!!!!!!!!!!!!!!!!!!!!!!!1\n",__func__,(u64)mapping,(u64)p_file_area);
            if(xa_is_value(p_file_area))
				break;
			if (xa_is_sibling(p_file_area))
				break;
        }
		p_file_area = entry_to_file_area(p_file_area);

		
		FILE_AREA_PRINT("%s mapping:0x%llx p_file_area:0x%llx xas.xa_index:%ld xas->xa_offset:%d index:%ld\n",__func__,(u64)mapping,(u64)p_file_area,xas.xa_index,xas.xa_offset,index);

find_page_from_file_area:
		folio = rcu_dereference(p_file_area->pages[page_offset_in_file_area]);
		/*如果fiolio是file_area的索引，则对folio清NULL，避免folio干扰后续判断*/
		folio_is_file_area_index_or_shadow_and_clear_NULL(folio);
		/*这个跟filemap_get_read_batch()里for (folio = xas_load(&xas); folio; folio = xas_next(&xas))判断出folio是NULL则结束循环是一个效果*/
		if(!folio)
			break;

		folio_index_from_xa_index = (xas.xa_index << PAGE_COUNT_IN_AREA_SHIFT) + page_offset_in_file_area;

		/*查找的page超过最大索引*/
		if(folio_index_from_xa_index > max )
			break;

        /*如果获取的page引用计数是0，说明已经被其他进程释放了。则直接goto retry从xarray tree按照老的xas.xa_index重新查找
		 *file_area，然后查找page。其实没有必要重新查找file_area，直接goto find_page_from_file_area重新获取page就行了!!!!!!!!!!*/
		if (!folio_try_get_rcu(folio)){
            printk("%s mapping:0x%llx folio:0x%llx index:%ld !folio_try_get_rcu(folio)\n",__func__,(u64)mapping,(u64)folio,folio->index);
			goto retry;//goto find_page_from_file_area;
		}

	    if (unlikely(folio != rcu_dereference(p_file_area->pages[page_offset_in_file_area]))){
            printk("%s mapping:0x%llx folio:0x%llx index:%ld folio != rcu_dereference(p_file_area->pages[page_offset_in_file_area]\n",__func__,(u64)mapping,(u64)folio,folio->index);
			/*当前page获取失败，把folio_put(folio)释放引用计数放到这里，然后goto next_folio分支，直接获取下一个page。这个思路错了。
			 *原版filemap_get_read_batch()函数在重新获取page异常后，是重新去xarray tree查找page，这里也要goto put_folio，
			 *然后执行xas_reset(&xas)重置xas，然后按照当前xas->xa_index和xas->xa_offset重新查找file_area，
			 再按照当前page_offset_in_file_area重新查找page。要理解 filemap_get_read_batch()函数查找page的原则，遇到非法page
			 要么尝试重新查找，要么立即break，不会一直向后查找而找到超出最大索引而break。这点跟find_get_entrie()原理不一样*/
			
			goto put_folio;
        }
		CHECK_FOLIO_FROM_FILE_AREA_VALID(&xas,mapping,folio,p_file_area,page_offset_in_file_area,folio_index_from_xa_index);

        FILE_AREA_PRINT("%s mapping:0x%llx folio:0x%llx index:%ld page_offset_in_file_area:%d\n",__func__,(u64)mapping,(u64)folio,folio->index,page_offset_in_file_area);

		if (!folio_batch_add(fbatch, folio))
			break;
		/*执行到这里，才真正获取到当前folio，然后才能令page_offset_in_file_area加1。但为了兼容还是加1放到next_folio那里了。
		 *但是在if (!folio_test_uptodate(folio))和if (folio_test_readahead(folio))两个成功获取page但break终止的分支都额外添加加1了*/
		if (!folio_test_uptodate(folio)){
			page_offset_in_file_area ++;
			break;
		}
		if (folio_test_readahead(folio)){
			page_offset_in_file_area ++;
			break;
		}
        /*folio代表单个page时，看着本质是xas->xa_index = folio->index，xas->xa_offset= folio->index & XA_CHUNK_MASK。
		 *这里的核心操作是，当folio->index大于64时，folio->index & XA_CHUNK_MASK后只取出不足64的部分，即在xarray tree槽位的偏移.
		 *但是folio = xas_next(&xas)里会判断出xas->xa_offset == 63后，会自动取下一个父节点查找page*/		
		//xas_advance(&xas, folio_next_index(folio) - 1);
        
		page_offset_in_file_area ++;

		/*如果file_area里还有page没遍历到，goto find_page_from_file_area去查找file_area里的下一个page。否则到for循环开头
		 *xas_for_each()去查找下一个file_area，此时需要find_page_from_file_area清0，这个很关键*/
		if(page_offset_in_file_area < PAGE_COUNT_IN_AREA){
			/*page_offset_in_file_area加1不能放到这里，重大逻辑错误。比如，上边判断page_offset_in_file_area是3的folio，
			 *然后执行到f(page_offset_in_file_area < PAGE_COUNT_IN_AREA)判断时，正常就应该不成立的，因为file_area的最后一个folio已经遍历过了*/
			goto find_page_from_file_area;
		}
		else{
			/* file_area的page都被访问完，则更新file_area的age和access_count。但新版本不管file_area的page被访问几个，
			 * 都视file_area访问一次，主要是为了降低性能损耗*/
			hot_file_update_file_status(mapping,p_file_stat_base,p_file_area,FILE_AREA_PAGE_IS_READ);

			//要查找下一个file_area了，page_offset_in_file_area要清0
			page_offset_in_file_area = 0;
		}

		continue;
put_folio:
		folio_put(folio);
retry:
		xas_reset(&xas);
	}
	
	/*如果前边for循环异常break了，就无法统计最后file_area的访问计数了，那就在这里统计。
	  但只有page_offset_in_file_area大于0，才能执行hot_file_update_file_status()。page_offset_in_file_area
	  非0说明上边的for循环是file_area的page没有遍历完就异常break了，没有对page_offset_in_file_area清0，
	  也就没有在上边的for循环执行hot_file_update_file_status()更新file_area的age和access_count。但是有
	  个特殊情况，初始值page_offset_in_file_area是1，然后上边遍历file_area时的第一个page时，因为page内存
	  被回收了，导致上边异常break，这个page这里并没有被访问，但是这里的if还会成立并更新file_area的age和
	  access_count。其实也没啥，这个page只是没有访问成功，但毕竟还是访问了。*/
	if(p_file_area && page_offset_in_file_area)
		hot_file_update_file_status(mapping,p_file_stat_base,p_file_area,FILE_AREA_PAGE_IS_READ);

	rcu_read_unlock();
}
EXPORT_SYMBOL(filemap_get_read_batch_for_file_area);
