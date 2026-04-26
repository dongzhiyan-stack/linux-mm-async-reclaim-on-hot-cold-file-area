#ifndef _ASYNC_MEMORY_RECLAIM_BASH_H_
#define _ASYNC_MEMORY_RECLAIM_BASH_H_
#include <linux/mm.h>
#include <linux/version.h>


#define CACHE_FILE_DELETE_PROTECT_BIT 0
#define MMAP_FILE_DELETE_PROTECT_BIT 1

#define FILE_AREA_IN_HOT_LIST 0
#define FILE_AREA_IN_REFAULT_LIST 1
#define FILE_AREA_IN_FREE_LIST 2

/*允许一个周期内file_stat->temp链表上file_area移动到file_stat->temp链表头的最大次数*/
#define FILE_AREA_MOVE_TO_HEAD_COUNT_MAX 16

/*一共3个bit位表示POS_NUM，最大到7*/
#define POS_WARM               0
#define POS_WIITEONLY_OR_COLD  1
#define POS_WARM_COLD          2
#define POS_WARM_MIDDLE        3
#define POS_WARM_MIDDLE_HOT    4
#define POS_WARM_HOT           5
#define POS_ZERO_PAGE           6


/*极小的文件，pagecache小于1M的文件*/
#define FILE_STAT_TINY_SMALL 0
/*小文件，pagecache在1M~10M的文件*/
#define FILE_STAT_SMALL 1
//普通文件，包含temp、middle、large 3类文件。pagecache在10M~30M是temp文件、30M~100M是middle文件，100M以上的是large文件 
#define FILE_STAT_NORMAL 2

/*file_area个数小于64是极小文件，在64~640是小文件。现在改为file_area个数小于32是极小文件，在32~640是小文件*/
#define SMALL_FILE_AREA_COUNT_LEVEL 64
//#define SMALL_FILE_AREA_COUNT_LEVEL 32
/*file_area个数在大于640(现改为128，pagecache 2M)，且小于1920是temp文件*/
//#define NORMAL_TEMP_FILE_AREA_COUNT_LEVEL 640
#define NORMAL_TEMP_FILE_AREA_COUNT_LEVEL 128
/*file_area个数在大于1920，且小于6400是middle文件*/
#define NORMAL_MIDDLE_FILE_AREA_COUNT_LEVEL 1920
/*file_area个数在大于6400是大型文件*/
#define NORMAL_LARGE_FILE_AREA_COUNT_LEVEL  6400

#define FILE_STAT_IN_TEMP_FILE_LIST 0 /*file_stat在普通文件链表*/
#define FILE_STAT_IN_MIDDLE_FILE_LIST 1 /*file_stat在普通文件链表*/
#define FILE_STAT_IN_LARGE_FILE_LIST 2 /*file_stat在普通大文件链表*/

#define TEMP_FILE (FILE_STAT_NORMAL + 1) /*file_stat的file_area个数是普通文件*/
#define MIDDLE_FILE (FILE_STAT_NORMAL + 2) /*file_stat的file_area个数是中型文件*/
#define LARGE_FILE (FILE_STAT_NORMAL + 3) /*file_stat的file_area个数是大文件*/

//一个file_stat结构里缓存的热file_area结构个数
#define FILE_AREA_CACHE_COUNT 3
//置1才允许异步内存回收
#define ASYNC_MEMORY_RECLAIM_ENABLE 0
//置1说明说明触发了drop_cache，此时禁止异步内存回收线程处理gloabl drop_cache_file_stat_head链表上的file_stat
#define ASYNC_DROP_CACHES 1
#define MEMORY_IN_RECLAIM 2
//异步内存回收周期，单位s
#define ASYNC_MEMORY_RECLIAIM_PERIOD 10
//最大文件名字长度
#define MAX_FILE_NAME_LEN 100
//当一个文件file_stat长时间不被访问，释放掉了所有的file_area，再过FILE_STAT_DELETE_AGE_DX个周期，则释放掉file_stat结构
#define FILE_STAT_DELETE_AGE_DX  10
//一个 file_area 包含的page数，默认4个
#define PAGE_COUNT_IN_AREA_SHIFT 2
#define PAGE_COUNT_IN_AREA (1UL << PAGE_COUNT_IN_AREA_SHIFT)//4
#define PAGE_COUNT_IN_AREA_MASK (PAGE_COUNT_IN_AREA - 1)//0x3

#define TREE_MAP_SHIFT	6
#define TREE_MAP_SIZE	(1UL << TREE_MAP_SHIFT)
#define TREE_MAP_MASK (TREE_MAP_SIZE - 1)
#define TREE_ENTRY_MASK 3
#define TREE_INTERNAL_NODE 1

/*热file_area经过FILE_AREA_HOT_to_TEMP_AGE_DX个周期后，还没有被访问，则移动到file_area_warm链表*/
#define FILE_AREA_HOT_TO_TEMP_AGE_DX  500
/*发生refault的file_area经过FILE_AREA_REFAULT_TO_TEMP_AGE_DX个周期后，还没有被访问，则移动到file_area_warm链表*/
#define FILE_AREA_REFAULT_TO_TEMP_AGE_DX 500
/*普通的file_area在FILE_AREA_TEMP_TO_COLD_AGE_DX个周期内没有被访问则被判定是冷file_area，然后释放这个file_area的page*/
#define FILE_AREA_TEMP_TO_COLD_AGE_DX  100
/*在file_stat->warm上的file_area经过file_area_warm_to_temp_age_dx个周期没有被访问，则移动到file_stat->temp链表*/
//#define FILE_AREA_WARM_TO_TEMP_AGE_DX  (FILE_AREA_TEMP_TO_COLD_AGE_DX + 10) 
/*一个冷file_area，如果经过FILE_AREA_FREE_AGE_DX个周期，仍然没有被访问，则释放掉file_area结构*/
#define FILE_AREA_FREE_AGE_DX  (FILE_AREA_TEMP_TO_COLD_AGE_DX + 600)
/*当一个file_area因多次访问被设置了ahead标记，经过FILE_AREA_AHEAD_CANCEL_AGE_DX个周期后file_area没有被访问，才会允许清理file_area的ahead标记*/
//#define FILE_AREA_AHEAD_CANCEL_AGE_DX (FILE_AREA_TEMP_TO_COLD_AGE_DX + 10)

/*当一个file_area在一个周期内访问超过FILE_AREA_HOT_LEVEL次数，则判定是热的file_area*/
#define FILE_AREA_HOT_LEVEL (PAGE_COUNT_IN_AREA << 2)
/*如果一个file_area在FILE_AREA_MOVE_HEAD_DX个周期内被访问了两次，然后才能移动到链表头*/
#define FILE_AREA_MOVE_HEAD_DX 5
/*如果连续FILE_AREA_CHECK_HOT_DX个周期cache file_area都被访问，判定file_area为热file_area*/
#define FILE_AREA_CHECK_HOT_DX 3
/*在file_stat被判定为热文件后，记录当时的global_age。在未来HOT_FILE_COLD_AGE_DX时间内该文件进去冷却期：hot_file_update_file_status()函数中
 *只更新该文件file_area的age后，然后函数返回，不再做其他操作，节省性能*/
#define HOT_FILE_COLD_AGE_DX 10


//一个冷file_area，如果经过FILE_AREA_FREE_AGE_DX个周期，仍然没有被访问，则释放掉file_area结构
#define MMAP_FILE_AREA_FREE_AGE_DX  (MMAP_FILE_AREA_TEMP_TO_COLD_AGE_DX + 600)
//发生refault的file_area经过FILE_AREA_REFAULT_TO_TEMP_AGE_DX个周期后，还没有被访问，则移动到file_area_temp链表
#define MMAP_FILE_AREA_REFAULT_TO_TEMP_AGE_DX 600
//普通的file_area在FILE_AREA_TEMP_TO_COLD_AGE_DX个周期内没有被访问则被判定是冷file_area，然后释放这个file_area的page
#define MMAP_FILE_AREA_TEMP_TO_COLD_AGE_DX  10//这个参数调的很小容易在file_area被内存回收后立即释放，这样测试了很多bug，先不要改

//file_area如果在 MMAP_FILE_AREA_HOT_AGE_DX 周期内被检测到访问 MMAP_FILE_AREA_ACCESS_HOT_COUNT 次，file_area被判定为热file_area
#define MMAP_FILE_AREA_ACCESS_HOT_COUNT 2
//hot链表上的file_area在MMAP_FILE_AREA_HOT_TO_TEMP_AGE_DX个周期内没有被访问，则降级到temp链表
#define MMAP_FILE_AREA_HOT_TO_TEMP_AGE_DX 600

//mapcount的file_area在MMAP_FILE_AREA_MAPCOUNT_AGE_DX个周期内不再遍历访问，降低性能损耗
#define MMAP_FILE_AREA_MAPCOUNT_AGE_DX 5
//hot链表上的file_area在MMAP_FILE_AREA_HOT_AGE_DX个周期内不再遍历访问，降低性能损耗
#define MMAP_FILE_AREA_HOT_AGE_DX 20
//free链表上的file_area在MMAP_FILE_AREA_HOT_AGE_DX个周期内不再遍历访问，降低性能损耗
//#define MMAP_FILE_AREA_FREE_AGE_DX 5
//refault链表上的file_area在MMAP_FILE_AREA_HOT_AGE_DX个周期内不再遍历访问，降低性能损耗
#define MMAP_FILE_AREA_REFAULT_AGE_DX 5

#define SUPPORT_FS_UUID_LEN UUID_SIZE
#define SUPPORT_FS_NAME_LEN 10
#define SUPPORT_FS_COUNT 2

#define SUPPORT_FS_ALL  0
#define SUPPORT_FS_UUID 1
#define SUPPORT_FS_SINGLE     2
#define FILE_AREA_IS_READ 1

#define FILE_AREA_PAGE_IS_READ 0
#define FILE_AREA_PAGE_IS_WRITE 1
#define FILE_AREA_PAGE_IS_READ_WRITE 2


/*cache文件file_stat的file_area包含mmap的文件页。必须是负数，怕跟其他file_stat类型的宏定义有充足*/
#define FILE_STAT_FROM_CACHE_FILE  -101
/*file_area来自file_stat->free、refault、hot链表，遍历时不能移动该file_area到其他file_stat链表，并且file_area不参与内存回收*/
#define FILE_STAT_OTHER_FILE_AREA (-102)

#define PRINT_FILE_STAT_INFO 0
#define UPDATE_FILE_STAT_REFAULT_COUNT 1

#define MEMORY_IDLE_SCAN  0 /*内存正常，常规的巡检*/
#define MEMORY_LITTLE_RECLAIM  1/*发现内存碎片，或者前后两个周期有大量内存分配*/
#define MEMORY_PRESSURE_RECLAIM  2/*zone free内存小于high阀值，有内存紧缺迹象*/
#define MEMORY_EMERGENCY_RECLAIM  3/*内存非常紧缺*/

#define IS_IN_MEMORY_IDLE_SCAN(p_hot_cold_file_global) (MEMORY_IDLE_SCAN == p_hot_cold_file_global->memory_pressure_level)
#define IS_IN_MEMORY_LITTLE_RECLAIM(p_hot_cold_file_global) (MEMORY_LITTLE_RECLAIM == p_hot_cold_file_global->memory_pressure_level)
#define IS_IN_MEMORY_PRESSURE_RECLAIM(p_hot_cold_file_global) (MEMORY_PRESSURE_RECLAIM == p_hot_cold_file_global->memory_pressure_level)
#define IS_IN_MEMORY_EMERGENCY_RECLAIM(p_hot_cold_file_global) (MEMORY_EMERGENCY_RECLAIM == p_hot_cold_file_global->memory_pressure_level)
#define IS_MEMORY_ENOUGH(p_hot_cold_file_global) (p_hot_cold_file_global->memory_pressure_level < MEMORY_PRESSURE_RECLAIM)

/**针对mmap文件新加的******************************/
#define MMAP_FILE_NAME_LEN 16
struct mmap_file_shrink_counter
{
	unsigned int scan_mapcount_file_area_count;
	unsigned int scan_hot_file_area_count;
	unsigned int find_cache_page_count_from_mmap_file;

	unsigned int scan_file_area_count_from_cache_file;
	unsigned int scan_cold_file_area_count_from_cache_file;
	unsigned int free_pages_from_cache_file;

	unsigned int mapcount_to_warm_file_area_count;
	unsigned int hot_to_warm_file_area_count;
	unsigned int refault_to_warm_file_area_count;
	unsigned int check_refault_file_area_count;
	unsigned int free_file_area_count;

	unsigned int isolate_lru_pages_from_warm;
	unsigned int scan_cold_file_area_count_from_warm;
	unsigned int warm_to_temp_file_area_count;

	unsigned int isolate_lru_pages_from_temp;
	unsigned int scan_cold_file_area_count_from_temp;
	unsigned int temp_to_warm_file_area_count;
	unsigned int temp_to_temp_head_file_area_count;
	unsigned int scan_file_area_count_file_move_from_cache;

	unsigned int scan_file_area_count;
	unsigned int scan_file_stat_count;

	unsigned int mapcount_to_temp_file_area_count_from_mapcount_file;

	unsigned int hot_to_temp_file_area_count_from_hot_file;

	unsigned int del_file_area_count;
	unsigned int del_file_stat_count;

	unsigned int mmap_free_pages_count;
	unsigned int writeback_count;
	unsigned int dirty_count;

	unsigned int file_area_hot_to_warm_list_count;
	unsigned int file_area_refault_to_warm_list_count;
	unsigned int file_area_free_count_from_free_list;
	
	unsigned int scan_read_file_area_count_from_temp;
	unsigned int temp_to_hot_file_area_count;
	unsigned int scan_ahead_file_area_count_from_temp;
	unsigned int warm_to_hot_file_area_count;
	unsigned int scan_file_area_count_from_warm;
	unsigned int scan_ahead_file_area_count_from_warm;
	unsigned int scan_read_file_area_count_from_warm;
	unsigned int file_area_hot_to_warm_from_hot_file;
	unsigned int del_zero_file_area_file_stat_count;
	unsigned int scan_zero_file_area_file_stat_count;
	unsigned int cache_file_stat_get_file_area_fail_count;
	unsigned int mmap_file_stat_get_file_area_from_cache_count;
	unsigned int scan_cold_file_area_count_from_mmap_file;
	unsigned int isolate_lru_pages_from_mmap_file;
	unsigned int isolate_lru_pages;
	unsigned int free_pages_count;
	unsigned int free_pages_from_mmap_file;
	unsigned int find_mmap_page_count_from_cache_file;
	unsigned int scan_delete_file_stat_count;
};
struct hot_cold_file_shrink_counter
{
	unsigned int find_mmap_page_count_from_cache_file;

	unsigned int del_zero_file_area_file_stat_count;
	unsigned int scan_zero_file_area_file_stat_count;

	unsigned int file_area_refault_to_warm_list_count;
	unsigned int file_area_hot_to_warm_list_count;
	unsigned int file_area_free_count_from_free_list;

	unsigned int scan_cold_file_area_count_from_temp;
	unsigned int scan_read_file_area_count_from_temp;
	unsigned int temp_to_hot_file_area_count;
	unsigned int scan_ahead_file_area_count_from_temp;
	unsigned int temp_to_warm_file_area_count;


	unsigned int scan_cold_file_area_count_from_warm;
	unsigned int scan_read_file_area_count_from_warm;            
	unsigned int scan_ahead_file_area_count_from_warm;
	unsigned int scan_file_area_count_from_warm;
	unsigned int warm_to_temp_file_area_count;
	unsigned int warm_to_hot_file_area_count;

	unsigned int scan_cold_file_area_count_from_mmap_file;
	unsigned int isolate_lru_pages_from_mmap_file;
	unsigned int free_pages_from_mmap_file;

	unsigned int file_area_hot_to_warm_from_hot_file;

	unsigned int isolate_lru_pages;

	unsigned int scan_file_area_count;
	unsigned int scan_file_stat_count;
	unsigned int scan_delete_file_stat_count;

	unsigned int del_file_area_count;
	unsigned int del_file_stat_count;
	
	unsigned int free_pages_count;
	unsigned int writeback_count;
	unsigned int dirty_count;

	unsigned int lru_lock_contended_count;
	
	unsigned int cache_file_stat_get_file_area_fail_count;
	unsigned int mmap_file_stat_get_file_area_from_cache_count;
	unsigned int scan_hot_file_area_count;
};

#ifdef __BIG_ENDIAN
#error "__BIG_ENDIAN not support!!!!!!!!!!!"
#endif
/*bit7表示file_area处于的hot链表，it8~bit15表示file_area所处的各种状态*/
#define FILE_AREA_LIST_VAILD_START_BIT 7
union warm_list_num_and_access_freq{
	unsigned char val;
	struct{
		unsigned char access_freq:4;
		unsigned char warm_list_num:3;
		unsigned char list_hot_bit:1;
	}val_bits;
};

//一个file_area表示了一片page范围(默认6个page)的冷热情况，比如page索引是0~5、6~11、12~17各用一个file_area来表示
struct file_area
{
	/* 要把struct list_head file_area_list放到file_area结构体首地址，如果将来遍历file_stat各种链表上的file_area时，
	 * 错把链表头当成file_area,container_of后得到p_file_area指向的是链表头首地址，后续p_file_area指向的内存，是
	 * file_stat结构体内部的，p_file_area->file_area_state会因状态非法而主动crash*/
	union{
		//file_area通过file_area_list添加file_stat的各种链表
		struct list_head file_area_list;
		//rcu_head和list_head都是16个字节
		struct rcu_head		i_rcu;
	};
	//不同取值表示file_area当前处于哪种链表
	union{
		/* bit0~bit3表示file_area的访问频次计数，bit4~bit6表示file_area所在warm_or_writeonly链表的编号，bit7表示file_area处于的hot链表.
		 * bit8~bit15表示file_area所处的各种状态 bit15~bit31表示file_area的page的各种状态*/
		union warm_list_num_and_access_freq warm_list_num_and_access_freq;
		unsigned int file_area_state;
	};
	//该file_area最近被访问时的global_age，长时间不被访问则与global age差很多，则判定file_area是冷file_area，然后释放该file_area的page
	//如果是mmap文件页，当遍历到文件页的pte置位，才会更新对应的file_area的age为全局age，否则不更新
	unsigned int file_area_age;

	//该file_area代表的N个连续page的起始page索引。为了节省内存，改为int类型，因此只能最大只能支持63.9T的文件产生的pagecache。是否要做个限制????????????????????????????
	unsigned int start_index;/*现在该为file_area的索引，不是对应的起始folio索引*/
	
	union{
		struct folio __rcu *pages[PAGE_COUNT_IN_AREA];
		/* global_file_stat链表上的file_area在文件iput()释放时，依照file_area->file_area_delete把file_area移动到
		 * global_file_stat->file_stat_delete链表。目的是这个list_move操作不用加锁，避免跟file_area.file_area_list
		 * 链表形成并发*/
		//struct list_head file_area_delete;
	};

	union{
		unsigned int refault_pages;
		unsigned int reserved;
		struct address_space *mapping;
	};
};
struct hot_cold_file_area_tree_node
{
	//与该节点树下最多能保存多少个page指针有关
	unsigned char   shift;
	//在节点在父节点中的偏移
	unsigned char   offset;
	//指向父节点
	struct hot_cold_file_area_tree_node *parent;
	//该节点下有多少个成员
	unsigned int    count;
	//是叶子节点时保存file_area结构，是索引节点时保存子节点指针
	void    *slots[TREE_MAP_SIZE];
};
struct hot_cold_file_area_tree_root
{
	unsigned int  height;//树高度
	struct hot_cold_file_area_tree_node __rcu *root_node;
};

struct file_stat_base
{
	struct address_space *mapping;
	union{
		//file_stat通过hot_cold_file_list添加到hot_cold_file_global的file_stat_hot_head链表
		struct list_head hot_cold_file_list;
		//rcu_head和list_head都是16个字节
		struct rcu_head		i_rcu;
	};
	//file_stat状态
	unsigned int file_stat_status;
	//总file_area个数
	unsigned int file_area_count;
	//file_stat锁
	spinlock_t file_stat_lock;

	union{
		//cache文件file_stat最近一次被异步内存回收访问时的age，调试用
		unsigned int recent_access_age;
		//mmap文件在扫描完一轮file_stat->temp链表上的file_area，进入冷却期，cooling_off_start_age记录当时的global age
		unsigned int cooling_off_start_age;
	};
	/*记录该文件file_stat被遍历时的全局age*/
	unsigned int recent_traverse_age;
	/*统计一个周期内file_stat->temp链表上file_area移动到file_stat->temp链表头的次数，每一个一次减1，减少到0则禁止
	 *file_stat->temp链表上file_area再移动到file_stat->temp链表头*/
	unsigned char file_area_move_to_head_count;
	unsigned int refault_page_count;
	unsigned int refault_page_count_last;


	//在文件file_stat->file_area_temp链表上的file_area个数
	unsigned int file_area_count_in_temp_list;
		
	//处于中间状态的file_area结构添加到这个链表，新分配的file_area就添加到这里
	struct list_head file_area_temp;
	/************base***base***base************/
}/*__attribute__((packed))*/;
struct file_stat_tiny_small
{
	struct file_stat_base file_stat_base;
	unsigned int reclaim_pages;
}/*__attribute__((packed))*/;

struct file_stat_small
{
	struct file_stat_base file_stat_base;
	/*hot、refault、free 等状态的file_area移动到这个链表*/
	struct list_head file_area_other;
	unsigned int reclaim_pages;
}/*__attribute__((packed))*/;
//热点文件统计信息，一个文件一个
struct file_stat
{
	struct file_stat_base file_stat_base;
	unsigned int file_area_hot_count;
	unsigned int mapcount_file_area_count;

	//频繁被访问的文件page对应的file_area存入这个头结点。新的方案，mapcount的file_area也移动到这个链表
	struct list_head file_area_hot;
	/*温热的file_area移动到这个链表*/
	struct list_head file_area_warm_hot;
	/*温冷的file_area移动到这个链表*/
	struct list_head file_area_warm;
	struct list_head file_area_warm_cold;
	/*只读的file_area、很长时间未访问的file_area移动到这个链表*/
	struct list_head file_area_writeonly_or_cold;

	//所有被释放内存page的file_area结构最后添加到这个链表，如果长时间还没被访问，就释放file_area结构。
	struct list_head file_area_free;
	/*file_stat回收的总page数*/
	unsigned int reclaim_pages;
	char traverse_warm_list_num;
}/*__attribute__((packed))*/;

/*hot_cold_file_node_pgdat结构体每个内存节点分配一个，内存回收前，从lruvec lru链表隔离成功page，移动到每个内存节点绑定的
 * hot_cold_file_node_pgdat结构的pgdat_page_list链表上.然后参与内存回收。内存回收后把pgdat_page_list链表上内存回收失败的
 * page在putback移动回lruvec lru链表。这样做的目的是减少内存回收失败的page在putback移动回lruvec lru链表时，可以减少
 * lruvec->lru_lock或pgdat->lru_lock加锁，详细分析见cold_file_isolate_lru_pages()函数。但实际测试时，内存回收失败的page是很少的，
 * 这个做法的意义又不太大!其实完全可以把参与内存回收的page移动到一个固定的链表也可以！*/
struct hot_cold_file_node_pgdat
{
	pg_data_t *pgdat;
	struct list_head pgdat_page_list;
	struct list_head pgdat_page_list_mmap_file;
};
struct reclaim_pages_counter{
	unsigned int tiny_small_file_stat_reclaim_pages;
	unsigned int small_file_stat_reclaim_pages;
	unsigned int temp_file_stat_reclaim_pages;
	unsigned int middle_file_stat_reclaim_pages;
	unsigned int large_file_stat_reclaim_pages;
	unsigned int writeonly_file_stat_reclaim_pages;
	unsigned int global_file_stat_reclaim_pages;
};
#define FILE_STAT_CACHE_FILE          0
#define FILE_STAT_MMAP_FILE           1
#define GLOBAL_FILE_STAT_CACHE_FILE   2
#define GLOBAL_FILE_STAT_MMAP_FILE    3

struct current_scan_file_stat_info{
	/*异步内存回收线程当前正在遍历的normal file_stat*/
	struct file_stat *p_traverse_file_stat;
	/* 这个指针依次从file_area_warm、file_area_writeonly_or_cold、file_area_hot链表尾向链表头指向一个个file_area
	 * ，判断file_area冷热程度移动到对应的链表*/
	struct file_area *p_traverse_first_file_area;
	/*指向正在遍历的file_area_warm、file_area_writeonly_or_cold等链表的链表头。定义这个变量是为了，实时直到当前遍历的哪个warm链表头的file_area*/
	struct list_head *p_traverse_file_area_list_head;
	/*file_area被判定热时，升级到更高一级的warm or hot链表*/
	struct list_head *p_up_file_area_list_head;
	/*file_area被判定冷时，降级到更低一级的warm or cold链表*/
	struct list_head *p_down_file_area_list_head;

	/* 遍历file_area_warm、file_area_writeonly_or_cold等链表file_area时，遇到访问过的file_area先移动到temp_head
	 * 链表，因为这些file_area非冷非热，不适合升级到高一级的warm链表，或者降级到低一级的warm链表。等遍历完当前链表的所有
	 * file_area，再把该链表的file_area移动回刚才遍历的链表头。这样做的目的是，让访问过的file_area
	 * 都集中在warm的链表头，链表尾都是没访问过的file_area。后续一旦内存紧张，直接从warm链表头遍历没有访问过的file_area
	 * 即可，有很大概率遍历到的是冷file_area，加快了遍历的效率，不用担心遍历到太多的热的file_area，影响遍历效率*/
	//struct list_head file_stat_tmp_head;
	struct list_head temp_head;
	/*指向正在遍历的file_area_warm、file_area_writeonly_or_cold等链表的编号*/
	char   traverse_list_num;
	char   up_list_num;
	char   down_list_num;
	char   traverse_file_stat_type;

	/* 异步内存回收线程当前正在遍历的normal file_stat->warm等链表的file_area时，遇到要要移动到
	 * 链表头的file_area，先临时移动到这个链表，等遍历完该链表的所有file_area再把这些file_area移动回file_stat->warm等链表*/
    //struct list_head tmp_file_stat_list_head;
	unsigned long move_to_head_file_area_count;
	unsigned long move_to_high_level_file_area_count;
	unsigned long move_to_low_level_file_area_count;
	unsigned long scan_writeonly_file_area_count;
	unsigned long scan_file_area_count;
};

/* 有个隐藏很深的问题，正常只会把cache文件的file_area转到global_file_stat的链表上，但是随后该文件变成mmap文件了。
 * 该文件再分配的的page和file_area都是mmap的了，再把这些file_area都移动到global_file_stat的链表，后续内存回收
 * 就会造成很大的麻烦，因为这些file_area不是cache page。至少global_file_stat得有mapcount链表。并且，后续该mmap
 * 的file_area都要添加到global_file_stat吗？最后决定还是添加到global_mmap_file_stat链表上。具体步骤是，
 * 在add_folio()函数里，检测到文件的mappint->rh_reserved1是global_file_stat，但该文件是mmap文件，则
 * 把global_mmap_file_stat结构体地址赋值给该文件mappint->rh_reserved1，如此该赋值生效前，该文件的mmap fil_area
 * 依然会移动到global_file_stat链表上(会很少)，赋值生效后，文件的mmap fil_area都会移动到global_mmap_file_stat
 * */
struct global_file_stat{
    struct file_stat file_stat;

	//struct list_head file_area_warm_cold;
	struct list_head file_area_warm_middle_hot;
	struct list_head file_area_warm_middle;

	struct list_head file_area_mapcount;
	struct list_head file_area_refault;
	
	struct list_head file_area_delete_list;
	struct list_head file_area_delete_list_temp;
	struct list_head zero_page_file_area_list;
	spinlock_t file_area_delete_lock;
	char traverse_file_stat_type;

	struct current_scan_file_stat_info current_scan_file_stat_info;
};
struct memory_reclaim_info_for_one_warm_list{
	unsigned int scan_file_area_count_in_reclaim;
	unsigned int scan_zero_page_file_area_count_in_reclaim;
	unsigned int scan_warm_file_area_count;
	unsigned int scan_file_area_count_reclaim_fail;
	unsigned int reclaim_pages_count;
};
struct memory_reclaim_info{
	unsigned int scan_file_area_count;
	unsigned int scan_file_area_max;

	unsigned int scan_exit_file_area_count;
	unsigned int scan_zero_page_file_area_count;

	unsigned int warm_list_file_area_up_count;
	unsigned int warm_list_file_area_down_count;

	unsigned int warm_list_file_area_to_writeonly_list_count;
	unsigned int warm_list_file_area_to_writeonly_list_count_cold;

	unsigned int direct_reclaim_pages_form_writeonly_file;
	unsigned int scan_file_area_count_form_writeonly_file;

	unsigned int scan_other_list_file_area_count;
	int scan_file_area_max_for_memory_reclaim;
	unsigned int scan_file_area_count_reclaim_fail;

	struct memory_reclaim_info_for_one_warm_list  memory_reclaim_info_writeonly_list;
	struct memory_reclaim_info_for_one_warm_list  memory_reclaim_info_warm_cold_list;
	struct memory_reclaim_info_for_one_warm_list  memory_reclaim_info_warm_middle_list;
	struct memory_reclaim_info_for_one_warm_list  memory_reclaim_info_warm_list;
	struct memory_reclaim_info_for_one_warm_list  memory_reclaim_info_direct_reclaim;
};
#define CURRENT_SCAN_FILE_STAT_INFO_TEMP 0
#define CURRENT_SCAN_FILE_STAT_INFO_MIDDLE 1
#define CURRENT_SCAN_FILE_STAT_INFO_LARGE 2
#define CURRENT_SCAN_FILE_STAT_INFO_WRITEONLY 3
/*真TM傻逼，要定义一个有4个成员的数组，竟然直接用CURRENT_SCAN_FILE_STAT_INFO_MAX(3)，好低级的错误，不过脑子的思考就容易犯错呀!!!!!!!!*/
//#define CURRENT_SCAN_FILE_STAT_INFO_MAX  CURRENT_SCAN_FILE_STAT_INFO_WRITEONLY
#define CURRENT_SCAN_FILE_STAT_INFO_MAX  (CURRENT_SCAN_FILE_STAT_INFO_WRITEONLY + 1)

#define MAX_PAGES_ZONE 0
#define SECOND_PAGES_ZONE 1
#define THIRD_PAGES_ZONE 2
#define MAX_ZONE (THIRD_PAGES_ZONE + 1)
//热点文件统计信息全局结构体
struct hot_cold_file_global
{
	struct zone *zone[MAX_ZONE];
	struct zone *normal_zone;
	unsigned int normal_zone_high_wmark_reclaim;
	unsigned int is_memory_idle_but_normal_zone_memory_tiny_count;

	unsigned int file_stat_in_move_free_list_file_area_count;
	unsigned int free_pages_from_cache_global_writeonly_or_cold_list;
	unsigned int free_pages_from_cache_global_warm_cold_list;
	unsigned int free_pages_from_cache_global_warm_middle_list;
	unsigned int free_pages_from_cache_global_warm_list;
	unsigned int free_pages_from_mmap_global_writeonly_or_cold_list;
	unsigned int free_pages_from_mmap_global_warm_cold_list;
	unsigned int free_pages_from_mmap_global_warm_middle_list;
	unsigned int free_pages_from_mmap_global_warm_list;
	
	unsigned int free_pages_from_cache_writeonly_or_cold_list;
	unsigned int free_pages_from_cache_warm_cold_list;
	unsigned int free_pages_from_cache_warm_middle_list;
	unsigned int free_pages_from_cache_warm_list;
	unsigned int free_pages_from_mmap_writeonly_or_cold_list;
	unsigned int free_pages_from_mmap_warm_cold_list;
	unsigned int free_pages_from_mmap_warm_middle_list;
	unsigned int free_pages_from_mmap_warm_list;

	unsigned int try_to_unmap_page_fail_count;
	unsigned int memory_tiny_count;
	/*控制判断内存紧张的内存zone 阈值*/
	unsigned int memory_zone_solve_age_order;
	/* 目前主要针对mmap文件的file_area，在最终内存回收前，如果file-area的age_dx大于file_stat_file_area_free_age_dx才允许回收该file_area
	 * 每个文件内存回收前都要对该变量清0*/
	unsigned int file_stat_file_area_free_age_dx;
	/*指向每次遍历的current_scan_file_stat_info结构体，调试用*/
	struct current_scan_file_stat_info *p_struct_current_scan_file_stat_info;
	struct memory_reclaim_info memory_reclaim_info;
	unsigned int alreay_reclaim_pages;
	unsigned int reclaim_pages_target;
	unsigned int cache_file_warm_list_file_area_up_count;
	unsigned int mmap_file_warm_list_file_area_up_count;
	unsigned int cache_file_warm_list_file_area_down_count;
	unsigned int mmap_file_warm_list_file_area_down_count;
	struct global_file_stat global_file_stat;
	struct global_file_stat global_mmap_file_stat;
	struct current_scan_file_stat_info current_scan_file_stat_info[CURRENT_SCAN_FILE_STAT_INFO_MAX];
	struct current_scan_file_stat_info current_scan_mmap_file_stat_info[CURRENT_SCAN_FILE_STAT_INFO_MAX];
	struct reclaim_pages_counter reclaim_pages_counter_cache;
	struct reclaim_pages_counter reclaim_pages_counter_mmap;
	unsigned int read_file_area_count_in_reclaim;
	unsigned int cold_file_area_delete_count;

	/*被判定是热文本的file_stat添加到file_stat_hot_head链表,超过50%或者80%的file_area都是热的，则该文件就是热文件，
	 * 文件的file_stat要移动到global的file_stat_hot_head链表*/
	struct list_head file_stat_hot_head;
	//新分配的文件file_stat默认添加到file_stat_temp_head链表
	struct list_head file_stat_temp_head;
	struct list_head file_stat_small_file_head;
	struct list_head file_stat_tiny_small_file_head;
	struct list_head file_stat_tiny_small_file_one_area_head;
	/*中等大小文件移动到这个链表*/
	struct list_head file_stat_middle_file_head;
	/*如果文件file_stat上的page cache数太多，被判定为大文件，则把file_stat移动到这个链表。将来内存回收时，优先遍历这种file_stat，
	 *因为file_area足够多，能遍历到更多的冷file_area，回收到内存page*/
	struct list_head file_stat_large_file_head;
	struct list_head file_stat_writeonly_file_head;
	struct list_head cold_file_head;
	//inode被删除的文件的file_stat移动到这个链表
	struct list_head file_stat_delete_head;
	struct list_head file_stat_small_delete_head;
	struct list_head file_stat_tiny_small_delete_head;
	//0个file_area的file_stat移动到这个链表
	struct list_head file_stat_zero_file_area_head;
	//struct list_head file_stat_middle_zero_file_area_head;
	//struct list_head file_stat_large_zero_file_area_head;
	struct list_head file_stat_small_zero_file_area_head;
	struct list_head file_stat_tiny_small_zero_file_area_head;
	//触发drop_cache后的没有file_stat的文件，分配file_stat后保存在这个链表
	struct list_head drop_cache_file_stat_head;

	//触发drop_cache后的没有file_stat的文件个数
	unsigned int drop_cache_file_count;
	//热文件file_stat个数
	unsigned int file_stat_hot_count;
	unsigned int mmap_file_stat_hot_count;
	//大文件file_stat个数
	unsigned int file_stat_large_count;
	unsigned int file_stat_middle_count;
	//文件file_stat个数
	unsigned int file_stat_count;
	unsigned int file_stat_small_count;
	unsigned int file_stat_tiny_small_count;
	//0个file_area的file_stat个数
	unsigned int file_stat_count_zero_file_area;
	unsigned int file_stat_small_count_zero_file_area;
	unsigned int file_stat_tiny_small_count_zero_file_area;

	/*当file_stat的file_area个数达到file_area_level_for_large_file时，表示该文件的page cache数太多，被判定为大文件。但一个file_area
	 *包含了多个page，一个file_area并不能填满page，因此实际file_stat的file_area个数达到file_area_level_for_large_file时，实际该文件的的page cache数会少点*/
	unsigned int file_area_level_for_large_file;
	unsigned int file_area_level_for_middle_file;
	//当一个文件的文件页page数大于nr_pages_level时，该文件的文件页page才会被本异步内存回收模块统计访问频率并回收，默认15，即64k，可通过proc接口调节大小
	unsigned int nr_pages_level;

	struct kmem_cache *file_stat_cachep;
	struct kmem_cache *file_stat_small_cachep;
	struct kmem_cache *file_stat_tiny_small_cachep;

	struct kmem_cache *file_area_cachep;
	//保存文件file_stat所有file_area的radix tree
	struct kmem_cache *hot_cold_file_area_tree_node_cachep;
	struct hot_cold_file_node_pgdat *p_hot_cold_file_node_pgdat;
	//异步内存回收线程，每个周期运行一次
	struct task_struct *hot_cold_file_thead;
	//负责内存回收，由hot_cold_file_thead线程唤醒，才会进行内存回收
	struct task_struct *async_memory_reclaim;
	int node_count;

	//有多少个进程在执行hot_file_update_file_status函数使用文件file_stat、file_area
	atomic_t   ref_count;
	//有多少个进程在执行__destroy_inode_handler_post函数，正在删除文件inode
	atomic_t   inode_del_count;
	//内存回收各个参数统计
	struct hot_cold_file_shrink_counter hot_cold_file_shrink_counter;
	//proc文件系统根节点
	struct proc_dir_entry *hot_cold_file_proc_root;

	spinlock_t global_lock;
	//全局age，每个周期加1
	unsigned int global_age;
	//异步内存回收周期，单位s
	unsigned int global_age_period;
	//热file_area经过file_area_refault_to_temp_age_dx个周期后，还没有被访问，则移动到file_area_temp链表
	unsigned int file_area_hot_to_temp_age_dx;
	unsigned int file_area_hot_to_temp_age_dx_ori;
	//发生refault的file_area经过file_area_refault_to_temp_age_dx个周期后，还没有被访问，则移动到file_area_temp链表
	unsigned int file_area_refault_to_temp_age_dx;
	unsigned int file_area_refault_to_temp_age_dx_ori;
	//普通的file_area在file_area_temp_to_cold_age_dx个周期内没有被访问则被判定是冷file_area，然后释放这个file_area的page
	unsigned int file_area_temp_to_cold_age_dx;
	unsigned int file_area_temp_to_cold_age_dx_ori;
	//普通的file_area在file_area_temp_to_warm_age_dx个周期内没有被访问则被判定是温file_area，然后把这个file_area移动到file_stat->file_area_warm链表
	unsigned int file_area_temp_to_warm_age_dx;
	unsigned int file_area_temp_to_warm_age_dx_ori;
	/*在file_stat->warm上的file_area经过file_area_warm_to_temp_age_dx个周期没有被访问，则移动到file_stat->temp链表*/
	unsigned int file_area_warm_to_temp_age_dx;
	unsigned int file_area_warm_to_temp_age_dx_ori;
	/*正常情况不会回收read属性的file_area的page，但是如果该file_area确实很长很长很长时间没访问，也参与回收*/
	unsigned int file_area_reclaim_read_age_dx;
	unsigned int file_area_reclaim_read_age_dx_ori;
	unsigned int file_area_reclaim_ahead_age_dx;
	unsigned int file_area_reclaim_ahead_age_dx_ori;
	unsigned int file_area_cold_level;
	unsigned int to_down_list_age_dx;
	unsigned int to_writeonly_cold_list_age_dx;
	//一个冷file_area，如果经过file_area_free_age_dx_fops个周期，仍然没有被访问，则释放掉file_area结构
	unsigned int file_area_free_age_dx;
	unsigned int file_area_free_age_dx_ori;
	//当一个文件file_stat长时间不被访问，释放掉了所有的file_area，再过file_stat_delete_age_dx个周期，则释放掉file_stat结构
	unsigned int file_stat_delete_age_dx;
	/*一个周期内，运行一个文件file_stat->temp链表头向前链表头移动的file_area个数*/
	unsigned int file_area_move_to_head_count_max;

	//发生refault的次数,累加值
	unsigned long all_refault_count;
		

	char support_fs_type;
	char support_fs_uuid[SUPPORT_FS_COUNT][SUPPORT_FS_UUID_LEN];
	char support_fs_against_uuid[SUPPORT_FS_UUID_LEN];
	char support_fs_name[SUPPORT_FS_COUNT][SUPPORT_FS_NAME_LEN];

	/**针对mmap文件新增的****************************/
	//新分配的文件file_stat默认添加到file_stat_temp_head链表
	struct list_head mmap_file_stat_uninit_head;
	//当一个文件的page都遍历完后，file_stat移动到这个链表
	struct list_head mmap_file_stat_temp_head;
	struct list_head mmap_file_stat_small_file_head;
	struct list_head mmap_file_stat_tiny_small_file_head;
	struct list_head mmap_file_stat_tiny_small_file_one_area_head;
	struct list_head mmap_file_stat_middle_file_head;
	//文件file_stat个数超过阀值移动到这个链表
	struct list_head mmap_file_stat_large_file_head;
	//热文件移动到这个链表
	struct list_head mmap_file_stat_hot_head;
	//一个文件有太多的page的mmapcount都大于1，则把该文件file_stat移动该链表
	struct list_head mmap_file_stat_mapcount_head;
	//0个file_area的file_stat移动到这个链表，暂时没用到
	struct list_head mmap_file_stat_zero_file_area_head;
	struct list_head mmap_file_stat_small_zero_file_area_head;
	struct list_head mmap_file_stat_tiny_small_zero_file_area_head;
	//inode被删除的文件的file_stat移动到这个链表，暂时不需要
	struct list_head mmap_file_stat_delete_head;
	struct list_head mmap_file_stat_small_delete_head;
	struct list_head mmap_file_stat_tiny_small_delete_head;
	//每个周期频繁冗余lru_lock的次数
	unsigned int lru_lock_count;
	unsigned int mmap_file_lru_lock_count;

	//mmap文件用的全局锁
	spinlock_t mmap_file_global_lock;

	struct file_stat *file_stat_last;
	//mmap文件个数
	unsigned int mmap_file_stat_count;
	unsigned int mmap_file_stat_small_count;
	unsigned int mmap_file_stat_tiny_small_count;
	//mapcount文件个数
	unsigned int mapcount_mmap_file_stat_count;
	//热文件个数
	unsigned int hot_mmap_file_stat_count;
	struct mmap_file_shrink_counter mmap_file_shrink_counter;
	/*当file_stat的file_area个数达到file_area_level_for_large_mmap_file时，表示该文件的page cache数太多，被判定为大文件*/
	unsigned int mmap_file_area_level_for_large_file;
	unsigned int mmap_file_area_level_for_middle_file;

	unsigned int mmap_file_area_hot_to_temp_age_dx;
	unsigned int mmap_file_area_refault_to_temp_age_dx;
	unsigned int mmap_file_area_temp_to_cold_age_dx;
	unsigned int mmap_file_area_free_age_dx;
	unsigned int mmap_file_area_temp_to_warm_age_dx;
	unsigned int mmap_file_area_warm_to_temp_age_dx;
	unsigned int mmap_file_area_hot_age_dx;

	unsigned int normal_zone_free_pages_last;
	unsigned int dma32_zone_free_pages_last;
	unsigned int dma_zone_free_pages_last;
	unsigned int highmem_zone_free_pages_last;
	unsigned int normal1_zone_free_pages_last;
	/*内存紧张等级，越大表示内存越紧张，并且还会回收有read标记和ahead标记的file_area的page*/
	unsigned int memory_pressure_level;
	/*内存紧急模式，内存回收后，检测内存依然紧张*/
	unsigned int memory_still_memrgency_after_reclaim;
	
	//从系统启动到目前释放的page个数
	unsigned long free_pages;
	//从系统启动到目前释放的mmap page个数
	unsigned long free_mmap_pages;
	//在内存回收期间产生的refault file_area个数
	unsigned long check_refault_file_area_count;
	unsigned long check_refault_file_area_kswapd_count;
	unsigned long check_mmap_refault_file_area_count;
	unsigned long update_file_area_temp_list_count;
	unsigned long update_file_area_warm_list_count;
	unsigned long update_file_area_free_list_count;
	unsigned long file_area_refault_file;

	unsigned long update_file_area_other_list_count;
	unsigned long update_file_area_move_to_head_count;
	unsigned long update_file_area_hot_list_count;
	
	unsigned long file_stat_delete_protect;

	struct file_stat_base *print_file_stat;
	
	unsigned long tiny_small_file_stat_to_one_area_count;
	unsigned long file_stat_tiny_small_one_area_move_tail_count;
	unsigned long file_stat_tiny_small_move_tail_count;
	
	unsigned long kswapd_free_page_count;
	unsigned long async_thread_free_page_count;
	unsigned long kswapd_file_area_refault_file;
	//atomic_t   kswapd_file_area_refault_file;
	
	unsigned int refault_file_area_scan_dx;
	unsigned int reclaim_page_print_level;
	unsigned int refault_page_print_level;
	unsigned int writeonly_file_age_dx_ori;
	unsigned int writeonly_file_age_dx;
	unsigned int in_writeonly_list_file_count;
	/*统计每个内存回收周期回收的page数，每次内存回收前都清0*/
	unsigned int all_reclaim_pages_one_period;
	unsigned long warm_list_file_area_up_count;
	unsigned long warm_list_file_area_down_count;
	unsigned long warm_list_file_area_to_writeonly_list_count;
	unsigned long warm_list_file_area_to_writeonly_list_count_cold;
	unsigned long scan_exit_file_area_count;
	unsigned long scan_zero_page_file_area_count;
	unsigned long direct_reclaim_pages_form_writeonly_file;
};

enum file_area_status{
	F_file_area_in_hot_list = FILE_AREA_LIST_VAILD_START_BIT,//7
	
	F_file_area_in_free_list,
	F_file_area_in_refault_list,
	/*file_area对应的page的pagecount大于0的，则把file_area移动到该链表*/
	F_file_area_in_mapcount_list,
	FILE_AREA_LIST_VAILD_END_BIT = F_file_area_in_mapcount_list,
	/* 为什么要增加in_mapping_exit属性，只要文件iput()后遍历到file_area，就设置file_area的in_mapping_exit状态。
	 * 后续再遇到这种file_area，要万分小心，绝对不能再对它cold_file_area_delete而从xarray tree剔除*/
	F_file_area_in_mapping_exit,

	F_file_area_in_read,//bit 12
	F_file_area_in_cache,
	F_file_area_in_mmap,	
	F_file_area_in_init,/*新分配file_area后立即设置该标记*/

};

#define MAX_FILE_AREA_LIST_BIT FILE_AREA_LIST_VAILD_END_BIT
//0XFFF &  ~0X7F = 0XF80
#define FILE_AREA_LIST_MASK ((1 << (MAX_FILE_AREA_LIST_BIT + 1)) - 1) & (~((1 << FILE_AREA_LIST_VAILD_START_BIT) - 1))
//清理file_area的状态，在哪个链表
#define CLEAR_FILE_AREA_LIST_STATUS(list_name) \
	static inline void clear_file_area_in_##list_name(struct file_area *p_file_area)\
{clear_bit_unlock(F_file_area_in_##list_name,(unsigned long *)(&p_file_area->file_area_state));}
//设置file_area在哪个链表的状态
#define SET_FILE_AREA_LIST_STATUS(list_name) \
	static inline void set_file_area_in_##list_name(struct file_area *p_file_area)\
{set_bit(F_file_area_in_##list_name,(unsigned long *)(&p_file_area->file_area_state));}
//测试file_area在哪个链表
#define TEST_FILE_AREA_LIST_STATUS(list_name) \
	static inline int file_area_in_##list_name(struct file_area *p_file_area)\
{return test_bit(F_file_area_in_##list_name,(unsigned long *)(&p_file_area->file_area_state));}

/*这个测试file_area_state的error状态，无法使用set_bit/clear_bit形式，需要特别注意!!!!!!!!!*/
#define TEST_FILE_AREA_LIST_STATUS_ERROR(list_name) \
	static inline int file_area_in_##list_name##_error(struct file_area *p_file_area)\
{return READ_ONCE(p_file_area->file_area_state) & (~(1 << F_file_area_in_##list_name) & FILE_AREA_LIST_MASK);}

#define FILE_AREA_LIST_STATUS(list_name)     \
	CLEAR_FILE_AREA_LIST_STATUS(list_name) \
	SET_FILE_AREA_LIST_STATUS(list_name)  \
	TEST_FILE_AREA_LIST_STATUS(list_name) \
	TEST_FILE_AREA_LIST_STATUS_ERROR(list_name)

//FILE_AREA_LIST_STATUS(temp_list)
FILE_AREA_LIST_STATUS(hot_list)
//FILE_AREA_LIST_STATUS(warm_list)
FILE_AREA_LIST_STATUS(free_list)
FILE_AREA_LIST_STATUS(refault_list)
FILE_AREA_LIST_STATUS(mapcount_list)

#define set_file_area_in_temp_list(p_file_area) {}
#define file_area_in_temp_list(p_file_area) (0 == (p_file_area->file_area_state & FILE_AREA_LIST_MASK))
#define clear_file_area_in_temp_list(p_file_area) {}
#define file_area_in_temp_list_error(p_file_area) (0 != (p_file_area->file_area_state & FILE_AREA_LIST_MASK))

	//清理file_area的状态，在哪个链表
#define CLEAR_FILE_AREA_STATUS(status) \
		static inline void clear_file_area_in_##status(struct file_area *p_file_area)\
{clear_bit_unlock(F_file_area_in_##status,(unsigned long *)(&p_file_area->file_area_state));}
	//设置file_area在哪个链表的状态
#define SET_FILE_AREA_STATUS(status) \
		static inline void set_file_area_in_##status(struct file_area *p_file_area)\
{set_bit(F_file_area_in_##status,(unsigned long *)(&p_file_area->file_area_state));}
	//测试file_area在哪个链表
#define TEST_FILE_AREA_STATUS(status) \
		static inline int file_area_in_##status(struct file_area *p_file_area)\
{return test_bit(F_file_area_in_##status,(unsigned long *)(&p_file_area->file_area_state));}

#define FILE_AREA_STATUS(status)     \
		CLEAR_FILE_AREA_STATUS(status) \
	SET_FILE_AREA_STATUS(status)  \
	TEST_FILE_AREA_STATUS(status) 

	FILE_AREA_STATUS(cache)
	FILE_AREA_STATUS(mmap)
FILE_AREA_STATUS(init)
	FILE_AREA_STATUS(read)
FILE_AREA_STATUS(mapping_exit)


#define file_area_in_writeonly_or_cold_list (p_file_area->file_area_state & FILE_AREA_LIST_MASK == 0)
#define file_area_in_writeonly_or_cold_list_invaild (p_file_area->file_area_state & FILE_AREA_LIST_MASK != 0)

#define file_area_in_temp_list_not_have_hot_status (1 << F_file_area_in_temp_list)
#define file_area_in_warm_list_not_have_hot_status (1 << F_file_area_in_warm_list)
#define file_area_in_free_list_not_have_refault_status (1 << F_file_area_in_free_list)


/*******file_stat状态**********************************************************/
enum file_stat_status{//file_area_state是long类型，只有64个bit位可设置
	F_file_stat_in_file_stat_hot_head_list,
	F_file_stat_in_file_stat_tiny_small_file_one_area_head_list,//只有一个file_area的file_stat_tiny_small移动到这个链表头
	F_file_stat_in_file_stat_tiny_small_file_head_list,
	F_file_stat_in_file_stat_small_file_head_list,

	F_file_stat_in_file_stat_temp_head_list,//
	F_file_stat_in_file_stat_middle_file_head_list,
	F_file_stat_in_file_stat_large_file_head_list,
    F_file_stat_in_file_stat_writeonly_file_head_list,//该文件只有write 的page，没有读page。如果是normal文件则移动到writeonly_normal链表，加快遍历到
	
	F_file_stat_in_mapcount_file_area_list,//文件file_stat是mapcount文件
	F_file_stat_in_zero_file_area_list,//
	F_file_stat_in_file_area_list_end = F_file_stat_in_zero_file_area_list,
	F_file_stat_in_cache_file,//cache文件，sysctl读写产生pagecache。有些cache文件可能还会被mmap映射，要与mmap文件互斥
	F_file_stat_in_mmap_file,//mmap文件，有些mmap文件可能也会被sysctl读写产生pagecache，要与cache文件互斥

	F_file_stat_in_global,
	F_file_stat_in_from_cache_file,//mmap文件是从cache文件的global temp链表移动过来的
	F_file_stat_in_test,
	F_file_stat_in_blacklist,//设置文件黑名单，不扫描内存回收

	/* tiny small文件如果file_area个数超过阈值，则在file_area_alloc_and_init()函数把file_stat_tiny_small移动到链表尾，
	 * 并设置in_tiny_small_to_tail标记。后续file_area_alloc_and_init()中就不会再移动到file_stat_tiny_small移动到链表尾*/
	F_file_stat_in_tiny_small_to_tail,
	/*该file_stat的file_area被移动到了current_scan_file_stat_info->tmp链表*/
	F_file_stat_in_file_area_in_tmp_list,
    F_file_stat_in_writeonly,//该文件只有write 的page，没有读page，这种文件即便file_area个数少也要移动到middel或large文件，内存回收优先回收这种文件。
	F_file_stat_invalid_start_index,

	F_file_stat_in_delete_file,//标识该file_stat被移动到了global delete链表	
	F_file_stat_in_delete,//仅仅表示该file_stat被触发delete了，并不能说明file_stat被移动到了global delete链表
	F_file_stat_in_replaced_file,//file_stat_tiny_small或file_stat_small转成更大的文件时，老的file_stat被标记replaced
	F_file_stat_in_move_free_list_file_area,
	F_file_stat_max_index,
};

//#define MAX_FILE_STAT_LIST_BIT F_file_stat_in_mapcount_file_area_list
#define MAX_FILE_STAT_LIST_BIT F_file_stat_in_zero_file_area_list
#define FILE_STAT_LIST_MASK ((1 << (MAX_FILE_STAT_LIST_BIT + 1)) - 1)

#define FILE_STAT_STATUS_INVALID_MASK (((1 << F_file_stat_max_index) - 1) & (~((1 << (F_file_stat_invalid_start_index + 1)) - 1)))
/*检测file_stat是否有异常状态，有的话就不能执行list_move_enhance()把本次遍历过的file_stat移动到链表头*/
#define file_stat_status_invalid_check(file_stat_status) (READ_ONCE(file_stat_status) & FILE_STAT_STATUS_INVALID_MASK)

	//清理file_stat的状态，在哪个链表
#define CLEAR_FILE_STAT_STATUS_BASE(name)\
		static inline void clear_file_stat_in_##name##_list_base(struct file_stat_base *p_file_stat_base)\
{clear_bit_unlock(F_file_stat_in_##name##_list,(unsigned long *)(&p_file_stat_base->file_stat_status));}
	//设置file_stat在哪个链表的状态
#define SET_FILE_STAT_STATUS_BASE(name)\
		static inline void set_file_stat_in_##name##_list_base(struct file_stat_base *p_file_stat_base)\
{set_bit(F_file_stat_in_##name##_list,(unsigned long *)(&p_file_stat_base->file_stat_status));}
	//测试file_stat在哪个链表
#define TEST_FILE_STAT_STATUS_BASE(name)\
		static inline int file_stat_in_##name##_list_base(struct file_stat_base *p_file_stat_base)\
{return test_bit(F_file_stat_in_##name##_list,(unsigned long *)(&p_file_stat_base->file_stat_status));}
#define TEST_FILE_STAT_STATUS_ERROR_BASE(name)\
		static inline int file_stat_in_##name##_list##_error_base(struct file_stat_base *p_file_stat_base)\
{return READ_ONCE(p_file_stat_base->file_stat_status) & (~(1 << F_file_stat_in_##name##_list) & FILE_STAT_LIST_MASK);}

#define FILE_STAT_STATUS_BASE(name) \
		CLEAR_FILE_STAT_STATUS_BASE(name) \
	SET_FILE_STAT_STATUS_BASE(name) \
	TEST_FILE_STAT_STATUS_BASE(name) \
	TEST_FILE_STAT_STATUS_ERROR_BASE(name)

FILE_STAT_STATUS_BASE(file_stat_hot_head)
FILE_STAT_STATUS_BASE(file_stat_temp_head)
FILE_STAT_STATUS_BASE(file_stat_middle_file_head)
FILE_STAT_STATUS_BASE(file_stat_large_file_head)
FILE_STAT_STATUS_BASE(file_stat_small_file_head)
FILE_STAT_STATUS_BASE(file_stat_tiny_small_file_head)
FILE_STAT_STATUS_BASE(file_stat_tiny_small_file_one_area_head)
FILE_STAT_STATUS_BASE(zero_file_area)
FILE_STAT_STATUS_BASE(file_stat_writeonly_file_head)
FILE_STAT_STATUS_BASE(mapcount_file_area)


	//清理文件的状态，大小文件等
#define CLEAR_FILE_STATUS_BASE(name)\
		static inline void clear_file_stat_in_##name##_base(struct file_stat_base *p_file_stat_base)\
{clear_bit_unlock(F_file_stat_in_##name,(unsigned long *)(&p_file_stat_base->file_stat_status));}
	//设置文件的状态，大小文件等
#define SET_FILE_STATUS_BASE(name)\
		static inline void set_file_stat_in_##name##_base(struct file_stat_base *p_file_stat_base)\
{set_bit(F_file_stat_in_##name,(unsigned long *)(&p_file_stat_base->file_stat_status));}
	//测试文件的状态，大小文件等
#define TEST_FILE_STATUS_BASE(name)\
		static inline int file_stat_in_##name##_base(struct file_stat_base *p_file_stat_base)\
{return test_bit(F_file_stat_in_##name,(unsigned long *)(&p_file_stat_base->file_stat_status));}
#define TEST_FILE_STATUS_ERROR_BASE(name)\
		static inline int file_stat_in_##name##_error_base(struct file_stat_base *p_file_stat_base)\
{return READ_ONCE(p_file_stat_base->file_stat_status) & (~(1 << F_file_stat_in_##name) & FILE_STAT_LIST_MASK);}

#define FILE_STATUS_BASE(name) \
		CLEAR_FILE_STATUS_BASE(name) \
	SET_FILE_STATUS_BASE(name) \
	TEST_FILE_STATUS_BASE(name)\
	TEST_FILE_STATUS_ERROR_BASE(name)

FILE_STATUS_BASE(delete)
FILE_STATUS_BASE(test)
FILE_STATUS_BASE(delete_file)
FILE_STATUS_BASE(cache_file)
FILE_STATUS_BASE(mmap_file)
FILE_STATUS_BASE(from_cache_file)
FILE_STATUS_BASE(replaced_file)
FILE_STATUS_BASE(blacklist)
FILE_STATUS_BASE(writeonly)
FILE_STATUS_BASE(global)
FILE_STATUS_BASE(tiny_small_to_tail)
FILE_STATUS_BASE(file_area_in_tmp_list)
FILE_STATUS_BASE(move_free_list_file_area)


/*设置/清除file_stat状态使用test_and_set_bit/clear_bit，是异步内存回收1.0版本的产物，现在不再需要。
 *因为设置/清理file_stat状态全都spin_lock加锁。并且会置/清除file_stat状态的只有两个场景，
 *一个是第1次创建file_stat添加到global temp链表时，这个有spin_lock加锁不用担心。第2个场景是，
 *文件被异步iput()释放file_stat并标记file_stat delete，此时也有spin_lock加锁，但是异步内存回收线程
 *遍历global temp/small/tiny small文件时，会spin_lock加锁情况下在is_file_stat_mapping_error()函数里
 *判断file_stat是否有delete标记。并且，异步内存回收线程会在对file_stat进行内存回收时判断file_stat是否有
 *delete标记。这就得非常注意了，因为file_stat的delete标记可能不会立即感知到，因此这两处异步内存回收线程
 *还通过内存屏障+其他变量辅助判断方法，判断file_stat是否有delete标记。后续，如果还有其他地方要判断
 *file_stat是否有delete标记，需要特别注意
 */

extern struct hot_cold_file_global hot_cold_file_global_info;

extern unsigned long async_memory_reclaim_status;
extern unsigned int file_area_in_update_count;
extern unsigned int file_area_in_update_lock_count;
extern unsigned int file_area_move_to_head_count;

extern unsigned int enable_xas_node_cache;
extern unsigned int enable_update_file_area_age;
extern int shrink_page_printk_open1;
extern int shrink_page_printk_open;
extern unsigned int xarray_tree_node_cache_hit;
extern int open_file_area_printk;
extern int open_file_area_printk_important;
extern int warm_list_printk;
extern int multi_level_file_area_printk;

#define is_global_file_stat_file_in_debug(mapping) (1 == mapping->rh_reserved2)
#define list_num_get(p_file_area)  (p_file_area->warm_list_num_and_access_freq.val_bits.warm_list_num)
#define file_area_access_freq(p_file_area)  (p_file_area->warm_list_num_and_access_freq.val_bits.access_freq)

/** file_area的page bit/writeback mark bit/dirty mark bit/towrite mark bit统计**************************************************************/
#define FILE_AREA_PAGE_COUNT_SHIFT (XA_CHUNK_SHIFT + PAGE_COUNT_IN_AREA_SHIFT)//6+2
#define FILE_AREA_PAGE_COUNT_MASK ((1 << FILE_AREA_PAGE_COUNT_SHIFT) - 1)//0xFF 

#define PAGE_BIT_OFFSET_IN_FILE_AREA_BASE (sizeof(unsigned int)*8 - PAGE_COUNT_IN_AREA)

/*writeback mark:bit27~bit24 dirty mark:bit23~bit20  towrite mark:bit19~bit16*/
#define WRITEBACK_MARK_IN_FILE_AREA_BASE (sizeof(unsigned int)*8 - PAGE_COUNT_IN_AREA*2)
#define DIRTY_MARK_IN_FILE_AREA_BASE     (sizeof(unsigned int)*8 - PAGE_COUNT_IN_AREA*3)
#define TOWRITE_MARK_IN_FILE_AREA_BASE   (sizeof(unsigned int)*8 - PAGE_COUNT_IN_AREA*4)

#if 0
#define FILE_AREA_PRINT(fmt,...) \
    do{ \
        if(open_file_area_printk) \
			printk(fmt,##__VA_ARGS__); \
	}while(0);

#define FILE_AREA_PRINT1(fmt,...) \
    do{ \
        if(open_file_area_printk_important) \
			printk(fmt,##__VA_ARGS__); \
	}while(0);
#else
#define FILE_AREA_PRINT(fmt,...)  {}
#define FILE_AREA_PRINT1(fmt,...) {}
#endif

#define  MULTI_LEVEL_FILE_AREA_PRINTK(fmt,...) \
    do{ \
        if(multi_level_file_area_printk) \
			printk(fmt,##__VA_ARGS__); \
	}while(0);

#define CHECK_FOLIO_FROM_FILE_AREA_VALID(xas,mapping,folio,p_file_area,page_offset_in_file_area,folio_index_from_xa_index) \
	if((folio)->index != (((p_file_area)->start_index << PAGE_COUNT_IN_AREA_SHIFT) + page_offset_in_file_area) || (folio)->index != folio_index_from_xa_index || (folio)->mapping != mapping)\
panic("%s xas:0x%llx file_area:0x%llx folio:0x%llx page_offset_in_file_area:%d mapping:0x%llx_0x%llx\n",__func__,(u64)xas,(u64)p_file_area,(u64)folio,page_offset_in_file_area,(u64)mapping,(u64)((folio)->mapping));

#define CHECK_FOLIO_FROM_FILE_AREA_VALID_MARK(xas,mapping,folio,folio_from_file_area,p_file_area,page_offset_in_file_area,folio_index_from_xa_index) \
	if(folio->index != (((p_file_area)->start_index << PAGE_COUNT_IN_AREA_SHIFT) + page_offset_in_file_area) || folio->index != folio_index_from_xa_index || folio != folio_from_file_area || (folio)->mapping != mapping)\
panic("%s xas:0x%llx file_area:0x%llx folio:0x%llx folio_from_file_area:0x%llx page_offset_in_file_area:%d mapping:0x%llx_0x%llx\n",__func__,(u64)xas,(u64)p_file_area,(u64)folio,(u64)folio_from_file_area,page_offset_in_file_area,(u64)mapping,(u64)((folio)->mapping));

inline static struct file_area *entry_to_file_area(void * file_area_entry)
{
	return (struct file_area *)((unsigned long)file_area_entry | 0x8000000000000000);
}
inline static void *file_area_to_entry(struct file_area *p_file_area)
{
	return (void *)((unsigned long)p_file_area & 0x7fffffffffffffff);
}
inline static int is_file_area_entry(void *file_area_entry)
{
	//最高的4个bit位依次是 0、1、1、1 则说明是file_area_entry，bit0和bit1也得是1
	return ((unsigned long)file_area_entry & 0xF000000000000003) == 0x7000000000000000;
}

inline static void clear_file_area_page_bit(struct file_area *p_file_area,unsigned char page_offset_in_file_area)
{	
	//page在 p_file_area->file_area_state对应的bit位清0
	if (!test_and_clear_bit(PAGE_BIT_OFFSET_IN_FILE_AREA_BASE + page_offset_in_file_area , (unsigned long *)&p_file_area->file_area_state))
		panic("%s file_area:0x%llx file_area_state:0x%x page_offset_in_file_area:%d already clear\n",__func__,(u64)p_file_area,p_file_area->file_area_state,page_offset_in_file_area);
}
inline static void set_file_area_page_bit(struct file_area *p_file_area,unsigned char page_offset_in_file_area)
{
	//page在 p_file_area->file_area_state对应的bit位置1
	if(test_and_set_bit(PAGE_BIT_OFFSET_IN_FILE_AREA_BASE + page_offset_in_file_area,(unsigned long *)&p_file_area->file_area_state))
		panic("%s file_area:0x%llx file_area_state:0x%x page_offset_in_file_area:%d already set\n",__func__,(u64)p_file_area,p_file_area->file_area_state,page_offset_in_file_area);
}
//测试page_offset_in_file_area这个位置的page在p_file_area->file_area_state对应的bit位是否置1了
/*这个测试file_area_state的error状态，无法使用set_bit/clear_bit形式，需要特别注意!!!!!!!!!*/
inline static int is_file_area_page_bit_set(struct file_area *p_file_area,unsigned char page_offset_in_file_area)
{
	unsigned int file_area_page_bit_set = (PAGE_BIT_OFFSET_IN_FILE_AREA_BASE + page_offset_in_file_area);

	return (test_bit(file_area_page_bit_set,(unsigned long *)&p_file_area->file_area_state));
}
/*这个测试file_area_state的error状态，无法使用set_bit/clear_bit形式，需要特别注意!!!!!!!!!*/
inline static int file_area_have_page(struct file_area *p_file_area)
{
	return  (READ_ONCE(p_file_area->file_area_state) & ~((1 << PAGE_BIT_OFFSET_IN_FILE_AREA_BASE) - 1));//0XF000 0000
}


/*探测file_area里的page是读还是写*/
inline static void set_file_area_page_read(struct file_area *p_file_area/*,unsigned char page_offset_in_file_area*/)
{
	set_file_area_in_read(p_file_area);
}
inline static void clear_file_area_page_read(struct file_area *p_file_area/*,unsigned char page_offset_in_file_area*/)
{
	clear_file_area_in_read(p_file_area);
}
inline static int file_area_page_is_read(struct file_area *p_file_area/*,unsigned char page_offset_in_file_area*/)
{
	return  file_area_in_read(p_file_area);
}

/*清理file_area所有的towrite、dirty、writeback的mark标记。这个函数是在把file_area从xarray tree剔除时执行的，之后file_area是无效的，有必要吗????????????*/
inline static void clear_file_area_towrite_dirty_writeback_mark(struct file_area *p_file_area)
{

}
inline static void clear_file_area_page_mark_bit(struct file_area *p_file_area,unsigned char page_offset_in_file_area,xa_mark_t type)
{
	unsigned int file_area_page_bit_clear;

	if(PAGECACHE_TAG_DIRTY == type){
		file_area_page_bit_clear =  DIRTY_MARK_IN_FILE_AREA_BASE + page_offset_in_file_area;
	}else if (PAGECACHE_TAG_WRITEBACK == type){
		file_area_page_bit_clear = WRITEBACK_MARK_IN_FILE_AREA_BASE + page_offset_in_file_area;
	}else{
		if(PAGECACHE_TAG_TOWRITE != type)
			panic("%s file_area:0x%llx file_area_state:0x%x page_offset_in_file_area:%d type:%d\n",__func__,(u64)p_file_area,p_file_area->file_area_state,page_offset_in_file_area,type);

		file_area_page_bit_clear = TOWRITE_MARK_IN_FILE_AREA_BASE + page_offset_in_file_area;
	}
	//page在 p_file_area->file_area_state对应的bit位清0
	clear_bit_unlock(file_area_page_bit_clear,(unsigned long *)&p_file_area->file_area_state);

}
inline static void set_file_area_page_mark_bit(struct file_area *p_file_area,unsigned char page_offset_in_file_area,xa_mark_t type)
{
	unsigned int file_area_page_mark_bit_set;

	if(PAGECACHE_TAG_DIRTY == type){
		file_area_page_mark_bit_set = DIRTY_MARK_IN_FILE_AREA_BASE + page_offset_in_file_area;
	}else if (PAGECACHE_TAG_WRITEBACK == type){
		file_area_page_mark_bit_set = WRITEBACK_MARK_IN_FILE_AREA_BASE + page_offset_in_file_area;
	}else{
		if(PAGECACHE_TAG_TOWRITE != type)
			panic("%s file_area:0x%llx file_area_state:0x%x page_offset_in_file_area:%d type:%d\n",__func__,(u64)p_file_area,p_file_area->file_area_state,page_offset_in_file_area,type);

		file_area_page_mark_bit_set = TOWRITE_MARK_IN_FILE_AREA_BASE + page_offset_in_file_area;
	}

	//page在 p_file_area->file_area_state对应的bit位置1
	set_bit(file_area_page_mark_bit_set,(unsigned long *)&p_file_area->file_area_state);
}
//测试page_offset_in_file_area这个位置的page在p_file_area->file_area_state对应的bit位是否置1了
inline static int is_file_area_page_mark_bit_set(struct file_area *p_file_area,unsigned char page_offset_in_file_area,xa_mark_t type)
{
	unsigned int file_area_page_mark_bit_set;

	if(PAGECACHE_TAG_DIRTY == type){
		file_area_page_mark_bit_set = DIRTY_MARK_IN_FILE_AREA_BASE + page_offset_in_file_area;
	}else if (PAGECACHE_TAG_WRITEBACK == type){
		file_area_page_mark_bit_set = WRITEBACK_MARK_IN_FILE_AREA_BASE + page_offset_in_file_area;
	}else{
		if(PAGECACHE_TAG_TOWRITE != type)
			panic("%s file_area:0x%llx file_area_state:0x%x page_offset_in_file_area:%d type:%d\n",__func__,(u64)p_file_area,p_file_area->file_area_state,page_offset_in_file_area,type);

		file_area_page_mark_bit_set = TOWRITE_MARK_IN_FILE_AREA_BASE + page_offset_in_file_area;
	}

	return (test_bit(file_area_page_mark_bit_set,(unsigned long *)&p_file_area->file_area_state));
}

/*统计有多少个 mark page置位了，比如file_area有3个page是writeback，则返回3*/
/*这个测试file_area_state的error状态，无法使用set_bit/clear_bit形式，需要特别注意!!!!!!!!!*/
inline static int file_area_page_mark_bit_count(struct file_area *p_file_area,char type)
{
	unsigned int file_area_page_mark;
	int count = 0;
	unsigned long page_mark_mask = (1 << PAGE_COUNT_IN_AREA) - 1;/*与上0xF，得到4个bit哪些置位0*/

	if(PAGECACHE_TAG_DIRTY == type){
		file_area_page_mark = (READ_ONCE(p_file_area->file_area_state) >> DIRTY_MARK_IN_FILE_AREA_BASE) & page_mark_mask;
	}else if (PAGECACHE_TAG_WRITEBACK == type){
		file_area_page_mark = (READ_ONCE(p_file_area->file_area_state) >> WRITEBACK_MARK_IN_FILE_AREA_BASE) & page_mark_mask;
	}else{
		if(PAGECACHE_TAG_TOWRITE != type)
			panic("%s file_area:0x%llx file_area_state:0x%x type:%d\n",__func__,(u64)p_file_area,p_file_area->file_area_state,type);

		file_area_page_mark = (READ_ONCE(p_file_area->file_area_state) >> TOWRITE_MARK_IN_FILE_AREA_BASE) & page_mark_mask;
	}
	while(file_area_page_mark){
		if(file_area_page_mark & 0x1)
			count ++;

		file_area_page_mark = file_area_page_mark >> 1;
	}

	return count;
}

inline static void is_cold_file_area_reclaim_support_fs(struct address_space *mapping,struct super_block *sb)
{
	if(SUPPORT_FS_ALL == hot_cold_file_global_info.support_fs_type){
		if(sb->s_type){
			if(0 == strcmp(sb->s_type->name,"ext4") || 0 == strcmp(sb->s_type->name,"xfs") || 0 == strcmp(sb->s_type->name,"f2fs"))
				mapping->rh_reserved1 = SUPPORT_FILE_AREA_INIT_OR_DELETE;
		}
	}
	else if(SUPPORT_FS_SINGLE == hot_cold_file_global_info.support_fs_type){
		if(sb->s_type){
			int i;
			for(i = 0;i < SUPPORT_FS_COUNT;i ++){
				if(0 == strcmp(sb->s_type->name,hot_cold_file_global_info.support_fs_name[i])){
					mapping->rh_reserved1 = SUPPORT_FILE_AREA_INIT_OR_DELETE;
					break;
				}
			}
		}
	}
	else if(SUPPORT_FS_UUID == hot_cold_file_global_info.support_fs_type){
		if(0 == memcmp(sb->s_uuid.b , hot_cold_file_global_info.support_fs_uuid[0], SUPPORT_FS_UUID_LEN) || 0 == memcmp(sb->s_uuid.b , hot_cold_file_global_info.support_fs_uuid[1], SUPPORT_FS_UUID_LEN)){
			if(memcmp(sb->s_uuid.b , hot_cold_file_global_info.support_fs_against_uuid, SUPPORT_FS_UUID_LEN))
			    mapping->rh_reserved1 = SUPPORT_FILE_AREA_INIT_OR_DELETE;
		}	
	}
}
extern int shrink_page_printk_open1;
extern int shrink_page_printk_open;
extern int shrink_page_printk_open_important;


#define folio_is_file_area_index_or_shadow(folio) xa_is_value(folio)

#define folio_is_file_area_index_or_shadow_and_clear_NULL(folio) \
{ \
	if(folio_is_file_area_index_or_shadow(folio))\
		folio = NULL; \
}

inline static void file_stat_delete_protect_lock(char is_cache_file)
{
	unsigned long lock_bit;
	if(is_cache_file)
		lock_bit = CACHE_FILE_DELETE_PROTECT_BIT;
	else
		lock_bit = MMAP_FILE_DELETE_PROTECT_BIT;

	while(test_and_set_bit_lock(lock_bit,&hot_cold_file_global_info.file_stat_delete_protect))
		cond_resched();
}
inline static int file_stat_delete_protect_try_lock(char is_cache_file)
{
	unsigned long lock_bit;
	if(is_cache_file)
		lock_bit = CACHE_FILE_DELETE_PROTECT_BIT;
	else
		lock_bit = MMAP_FILE_DELETE_PROTECT_BIT;

	return  (0 == test_and_set_bit_lock(lock_bit,&hot_cold_file_global_info.file_stat_delete_protect));
}
inline static void file_stat_delete_protect_unlock(char is_cache_file)
{
	unsigned long lock_bit;
	if(is_cache_file)
		lock_bit = CACHE_FILE_DELETE_PROTECT_BIT;
	else
		lock_bit = MMAP_FILE_DELETE_PROTECT_BIT;

	/*为了防护没有加锁就解锁，于是主动触发crash*/
	if(!test_and_clear_bit(lock_bit,&hot_cold_file_global_info.file_stat_delete_protect))
		BUG();
}
inline static void file_stat_delete_protect_test_unlock(char is_cache_file)
{
	unsigned long lock_bit;
	if(is_cache_file)
		lock_bit = CACHE_FILE_DELETE_PROTECT_BIT;
	else
		lock_bit = MMAP_FILE_DELETE_PROTECT_BIT;

	if(!test_and_clear_bit(lock_bit,&hot_cold_file_global_info.file_stat_delete_protect))
		BUG();
}

inline static unsigned int get_file_area_list_status(struct file_area *p_file_area)
{
	return p_file_area->file_area_state & FILE_AREA_LIST_MASK;
}
inline static unsigned int get_file_stat_normal_type_all(struct file_stat_base *p_file_stat_base)
{
	unsigned int file_stat_list_bit = p_file_stat_base->file_stat_status & FILE_STAT_LIST_MASK;

	switch (file_stat_list_bit){
		case 1 << F_file_stat_in_file_stat_temp_head_list:
		case 1 << F_file_stat_in_file_stat_middle_file_head_list:
		case 1 << F_file_stat_in_file_stat_large_file_head_list:
			return 1;

		default:
			return 0;
	}
	return 0;

}
inline static unsigned int get_file_stat_normal_type(struct file_stat_base *p_file_stat_base)
{
	unsigned int file_stat_list_bit = p_file_stat_base->file_stat_status & FILE_STAT_LIST_MASK;

	switch (file_stat_list_bit){
		case 1 << F_file_stat_in_file_stat_temp_head_list:
			return TEMP_FILE;
		case 1 << F_file_stat_in_file_stat_middle_file_head_list:
			return MIDDLE_FILE;
		case 1 << F_file_stat_in_file_stat_large_file_head_list:
			return LARGE_FILE;

		default:
			return -1;
	}
	return -1;

}
/*判断文件是否是tiny small文件、small文件、普通文件*/
inline static unsigned int get_file_stat_type_common(unsigned int file_stat_list_bit)
{
	switch (file_stat_list_bit){
		case 1 << F_file_stat_in_file_stat_small_file_head_list:
			return FILE_STAT_SMALL;
		case 1 << F_file_stat_in_file_stat_tiny_small_file_head_list:
		case 1 << F_file_stat_in_file_stat_tiny_small_file_one_area_head_list:
			return FILE_STAT_TINY_SMALL;

		case 1 << F_file_stat_in_file_stat_temp_head_list:
		case 1 << F_file_stat_in_file_stat_hot_head_list:
		case 1 << F_file_stat_in_file_stat_middle_file_head_list:
		case 1 << F_file_stat_in_file_stat_large_file_head_list:
		case 1 << F_file_stat_in_mapcount_file_area_list:
		case 1 << F_file_stat_in_file_stat_writeonly_file_head_list:
			return FILE_STAT_NORMAL;

		default:
			return -1;
	}
	return -1;
}

inline static unsigned int get_file_stat_type(struct file_stat_base *p_file_stat_base)
{
	unsigned int file_stat_list_bit = p_file_stat_base->file_stat_status & FILE_STAT_LIST_MASK;

	return get_file_stat_type_common(file_stat_list_bit);
}
inline static unsigned int get_file_stat_type_file_iput(struct file_stat_base *p_file_stat_base)
{
	unsigned int file_stat_list_bit = p_file_stat_base->file_stat_status & FILE_STAT_LIST_MASK;

	/* iput()文件时，遇到in_zero_list的file_stat，这种file_stat还保留了原始in_temp、in_middle等状态，
	 * 这里要清理掉file_stat的in_zero状态，否则get_file_stat_type()会返回-1，现在只返回in_temp、in_middle原始状态*/

	if(file_stat_list_bit & (1 << F_file_stat_in_zero_file_area_list))
		file_stat_list_bit &= ~(1 << F_file_stat_in_zero_file_area_list);

	return get_file_stat_type_common(file_stat_list_bit);
}
#define is_file_stat_match_error(p_file_stat_base,file_type) \
{ \
	if(get_file_stat_type(p_file_stat_base) != file_type)  \
	panic("%s file_stat:0x%llx match file_type:%d error\n",__func__,(u64)p_file_stat_base,file_type); \
}

#define is_file_stat_mapping_error(p_file_stat_base) \
do{ \
	rcu_read_lock();\
	if((unsigned long)p_file_stat_base != READ_ONCE((p_file_stat_base)->mapping->rh_reserved1)){  \
		smp_rmb();\
		if(file_stat_in_delete_base(p_file_stat_base)){\
			rcu_read_unlock(); \
			printk(KERN_WARNING "%s file_stat:0x%llx status:0x%x mapping:0x%llx delete!!!!!!!!!!!!\n",__func__,(u64)p_file_stat_base,(p_file_stat_base)->file_stat_status,(u64)((p_file_stat_base)->mapping)); \
			break;\
		} \
		else \
		panic("%s file_stat:0x%llx match mapping:0x%llx 0x%llx error\n",__func__,(u64)p_file_stat_base,(u64)((p_file_stat_base)->mapping),(u64)((p_file_stat_base)->mapping->rh_reserved1)); \
	}\
	rcu_read_unlock();\
}while(0);

inline static void file_stat_base_struct_init(struct file_stat_base *p_file_stat_base,char is_cache_file)
{
	//设置文件是mmap文件状态，有些mmap文件可能还会被读写，要与cache文件互斥，要么是cache文件要么是mmap文件，不能两者都是
	if(is_cache_file){
		set_file_stat_in_cache_file_base(p_file_stat_base);
		hot_cold_file_global_info.file_stat_tiny_small_count++;
		/*只有cache文件才设置writeonly标记*/
		set_file_stat_in_writeonly_base(p_file_stat_base);
	}
	else{
		set_file_stat_in_mmap_file_base(p_file_stat_base);
		hot_cold_file_global_info.mmap_file_stat_tiny_small_count++;
	}

	INIT_LIST_HEAD(&p_file_stat_base->file_area_temp);
	spin_lock_init(&p_file_stat_base->file_stat_lock); 
}
inline static void file_stat_base_init(struct address_space *mapping,struct file_stat_base *p_file_stat_base,char is_cache_file)
{
	mapping->rh_reserved1 = (unsigned long)(p_file_stat_base);
	p_file_stat_base->mapping = mapping;

	file_stat_base_struct_init(p_file_stat_base,is_cache_file);
	/*新分配的file_stat的recent_access_age赋值global age，否则就是0，可能会被识别为冷文件而迅速释放掉*/
	p_file_stat_base->recent_access_age = hot_cold_file_global_info.global_age;
}
inline static struct file_stat_base *file_stat_alloc_and_init_tiny_small(struct address_space *mapping,char is_cache_file)
{
	struct file_stat_tiny_small *p_file_stat_tiny_small = NULL;
	struct file_stat_base *p_file_stat_base = NULL;
	spinlock_t *p_global_lock;
	if(is_cache_file)
		p_global_lock = &hot_cold_file_global_info.global_lock;
	else
		p_global_lock = &hot_cold_file_global_info.mmap_file_global_lock;

	p_file_stat_tiny_small = kmem_cache_alloc(hot_cold_file_global_info.file_stat_tiny_small_cachep,GFP_ATOMIC);
	if (!p_file_stat_tiny_small) {
		printk("%s file_stat alloc fail\n",__func__);
		goto out;
	}
	memset(p_file_stat_tiny_small,0,sizeof(struct file_stat_tiny_small));

	spin_lock(p_global_lock);

	/*如果已经有进程并发分配了file_stat并执行file_stat_base_init()赋值给了mapping->rh_reserved1，那就释放掉本次分配的file_stat*/
	if(IS_SUPPORT_FILE_AREA_READ_WRITE(mapping)){
		spin_unlock(p_global_lock);

		kmem_cache_free(hot_cold_file_global_info.file_stat_tiny_small_cachep,p_file_stat_tiny_small);
		p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;

		printk("%s file_stat:0x%llx already alloc\n",__func__,(u64)mapping->rh_reserved1);
		goto out;
	}
	p_file_stat_base = &p_file_stat_tiny_small->file_stat_base;
	/* 对file_stat_base初始化，重点赋值mapping->rh_reserved1 = p_file_stat_base。这个赋值必须放到spin_lock加锁代码里，不能放到spin_unlock后。
	 * 否则，进程1 spin_unlock，还没赋值mapping->rh_reserved1 = p_file_stat_base。进程spin_lock成功，此时mapping->rh_reserved1还没赋值，
	 * 导致这里又把同一个文件的file_stat_tiny_small添加到mmap_file_stat_tiny_small_file_head链表，泄漏了，乱套了*/
	file_stat_base_init(mapping,p_file_stat_base,is_cache_file);
	set_file_stat_in_file_stat_tiny_small_file_head_list_base(p_file_stat_base);

	if(is_cache_file){
		hot_cold_file_global_info.file_stat_count++;
		list_add(&p_file_stat_base->hot_cold_file_list,&hot_cold_file_global_info.file_stat_tiny_small_file_head);
	}
	else{
		hot_cold_file_global_info.mmap_file_stat_tiny_small_count++;
		list_add(&p_file_stat_base->hot_cold_file_list,&hot_cold_file_global_info.mmap_file_stat_tiny_small_file_head);
	}

	spin_unlock(p_global_lock);


out:
	return p_file_stat_base;
}

inline static struct current_scan_file_stat_info *get_normal_file_stat_current_scan_file_stat_info(struct hot_cold_file_global *p_hot_cold_file_global,unsigned int file_stat_list_type,char is_cache_file)
{
	switch (file_stat_list_type){
		case (1 << F_file_stat_in_file_stat_temp_head_list):
			if(is_cache_file)
				return &p_hot_cold_file_global->current_scan_file_stat_info[CURRENT_SCAN_FILE_STAT_INFO_TEMP];
			else
				return &p_hot_cold_file_global->current_scan_mmap_file_stat_info[CURRENT_SCAN_FILE_STAT_INFO_TEMP];
		case (1 << F_file_stat_in_file_stat_middle_file_head_list):
			if(is_cache_file)
				return &p_hot_cold_file_global->current_scan_file_stat_info[CURRENT_SCAN_FILE_STAT_INFO_MIDDLE];
			else
				return &p_hot_cold_file_global->current_scan_mmap_file_stat_info[CURRENT_SCAN_FILE_STAT_INFO_MIDDLE];
		case (1 << F_file_stat_in_file_stat_large_file_head_list):
			if(is_cache_file)
				return &p_hot_cold_file_global->current_scan_file_stat_info[CURRENT_SCAN_FILE_STAT_INFO_LARGE];
			else
				return &p_hot_cold_file_global->current_scan_mmap_file_stat_info[CURRENT_SCAN_FILE_STAT_INFO_LARGE];
		case (1 << F_file_stat_in_file_stat_writeonly_file_head_list):
			if(is_cache_file)
				return &p_hot_cold_file_global->current_scan_file_stat_info[CURRENT_SCAN_FILE_STAT_INFO_WRITEONLY];
			else
				return &p_hot_cold_file_global->current_scan_mmap_file_stat_info[CURRENT_SCAN_FILE_STAT_INFO_WRITEONLY]; 
		default:
			panic("file_stat_list_type:0x%x is_cache_file:%d\n",file_stat_list_type,is_cache_file);
	}
}

/* 当current_scan_file_stat_info->p_traverse_file_stat指向的global->temp、middle、large、writeonly尾巴的链表file_stat，
 * 1:被iput()释放标记delete了、因file_area个数发生变化转成normal writeonly、temp、middle、large文件而文件状态变化
 * 2:当创建新的file_stat移动到global->temp、middle、large、writeonly链表尾巴，current_scan_file_stat_info->p_traverse_file_stat
 * 指向的file_stat不是global->temp、middle、large、writeonly链表尾的file_stat了，那就要清空
 * current_scan_file_stat_info->p_traverse_file_stat = NULL，令其失效，下边遍历global->temp、middle、large、writeonly链表尾的
 * file_stat时，再赋值给current_scan_file_stat_info->p_traverse_file_stat，这样才不会在
 * file_stat_multi_level_warm_or_writeonly_list_file_area_solve()函数里，因为current_scan_file_stat_info->p_traverse_file_stat
 * 跟global->temp、middle、large、writeonly链表尾的file_stat不相等而 panic。
 * 
 * 另外，仅仅更新下一次遍历该文件file_stat时，要遍历的warm链表编号，其他的p_traverse_file_area_list_head等信息在下次遍历到该文件file_stat时再更新*/
inline static void update_file_stat_next_multi_level_warm_or_writeonly_list(struct current_scan_file_stat_info *p_current_scan_file_stat_info,struct file_stat *p_file_stat)
{
	/*如果p_traverse_file_stat已经是NULL，说明p_traverse_file_stat已经没有指向遍历过的file_stat了，直接return*/
	if(NULL == p_current_scan_file_stat_info->p_traverse_file_stat)
		return;

	if(!file_stat_in_file_area_in_tmp_list_base(&p_current_scan_file_stat_info->p_traverse_file_stat->file_stat_base))
		panic("%s p_current_scan_file_stat_info:0x%llx p_file_stat:0x%llx error",__func__,(u64)p_current_scan_file_stat_info,(u64)p_file_stat);

	clear_file_stat_in_file_area_in_tmp_list_base(&p_current_scan_file_stat_info->p_traverse_file_stat->file_stat_base);

	/*1:当前file_stat的warm链表上的file_area完成了，更新next_num_list，还要把p_traverse_file_stat设置NULL，这样下次遍历同类型的
	 * file_stat时，才会更新到p_traverse_file_stat。同时，还要先把之前遍历过的移动到temp_head链表上的file_area移动回warm链表头
	 *2:当前file_stat的状态发生变化了，比如normal file_stat转成writeonly文件，或者temp、midle、large文件之间来回转换。执行同样的操作*/
	if(!list_empty(&p_current_scan_file_stat_info->temp_head)){
		list_splice_init(&p_current_scan_file_stat_info->temp_head,p_current_scan_file_stat_info->p_traverse_file_area_list_head);
	}
	/*普通文件的某个warm链表file_area遍历完后，必须把p_current_scan_file_stat_info->p_traverse_file_stat设置NULL，
	 * 然后下个周期遍历时，发现它是NULL，才会遍历的新的文件file_stat*/
	p_current_scan_file_stat_info->p_traverse_file_stat = NULL;
	p_current_scan_file_stat_info->p_traverse_file_area_list_head = NULL;
	p_current_scan_file_stat_info->p_traverse_first_file_area = NULL;
	p_current_scan_file_stat_info->p_up_file_area_list_head = NULL;
	p_current_scan_file_stat_info->p_down_file_area_list_head = NULL;

	switch(p_file_stat->traverse_warm_list_num){
		case POS_WARM_HOT:
			/* 遍历过file_stat->warm_hot链表上的file_area后，不再允许异步内存回收线程traverse_file_stat_multi_level_warm_list()遍历
			 * file_stat->writeonly链表上的file_area了。因为现在判定有点浪费性能，反正异步内存回收时，遍历file_stat->writeonly
			 * 链表上的file_area进行内存回收时。如果file_area最近访问过，直接移动到file_stat->warm链表。
			 * 但是针对mmap文件，writeonly链表上的file_area，异步内存回收线程不遍历该链表上的file_area，即使该file_area被访问了
			 * 也无法更新file_area_age，等内存紧张时就会被回收掉，于是决定mmap要遍历writeonly链表上的file_area。具体看update_global_file_stat_next_multi_level_warm_or_writeonly_list()*/
			if(file_stat_in_cache_file_base(&p_file_stat->file_stat_base))
				p_file_stat->traverse_warm_list_num = POS_WARM_COLD;
			else
				p_file_stat->traverse_warm_list_num = POS_WIITEONLY_OR_COLD;

			break;
		case POS_WARM:
			p_file_stat->traverse_warm_list_num = POS_WARM_HOT;
			break;
		case POS_WARM_COLD:
			p_file_stat->traverse_warm_list_num = POS_WARM;
			break;
		case POS_WIITEONLY_OR_COLD:
			p_file_stat->traverse_warm_list_num = POS_WARM_COLD;
			break;
		default:
			BUG();
	}
}

inline static struct file_stat_base *file_stat_alloc_and_init_other(struct address_space *mapping,unsigned int file_type,char free_old_file_stat,char is_cache_file,char is_writeonly_file)
{
	struct file_stat_base *p_file_stat_base = NULL;
	spinlock_t *p_global_lock;

	if(is_cache_file)
		p_global_lock = &hot_cold_file_global_info.global_lock;
	else
		p_global_lock = &hot_cold_file_global_info.mmap_file_global_lock;

	if(FILE_STAT_SMALL == file_type){
		struct file_stat_small *p_file_stat_small;

		p_file_stat_small = kmem_cache_alloc(hot_cold_file_global_info.file_stat_small_cachep,GFP_ATOMIC);
		if (!p_file_stat_small) {
			spin_unlock(&hot_cold_file_global_info.mmap_file_global_lock);
			printk("%s file_stat alloc fail\n",__func__);
			goto out;
		}
		memset(p_file_stat_small,0,sizeof(struct file_stat_small));

		spin_lock(p_global_lock);

		/*1:如果两个进程同时访问一个文件，同时执行到这里，需要加锁。第1个进程加锁成功后，分配file_stat并赋值给
		 *mapping->rh_reserved1，第2个进程获取锁后执行到这里mapping->rh_reserved1就会成立
		 *2:异步内存回收功能禁止了
		 *3:当small file_stat转到normal file_stat，释放老的small file_stat然后分配新的normal file_stat，此时
		 *free_old_file_stat 是1，下边的if不成立，忽略mapping->rh_reserved1，进而才不会goto out，而是分配新的file_stat
		 *mapping->rh_reserved1指向的老的file_stat另有代码释放掉这里不用管
		 */
		if(IS_SUPPORT_FILE_AREA_READ_WRITE(mapping) && !free_old_file_stat){
			spin_unlock(p_global_lock);
			
			kmem_cache_free(hot_cold_file_global_info.file_stat_small_cachep,p_file_stat_small);
			p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;

			printk("%s file_stat:0x%llx already alloc\n",__func__,(u64)mapping->rh_reserved1);
			goto out;  
		}

		p_file_stat_base = &p_file_stat_small->file_stat_base;
		file_stat_base_init(mapping,p_file_stat_base,is_cache_file);

		INIT_LIST_HEAD(&p_file_stat_small->file_area_other);

		set_file_stat_in_file_stat_small_file_head_list_base(p_file_stat_base);

		if(is_cache_file){
			hot_cold_file_global_info.file_stat_small_count++;
			/*writeonly文件要移动到链表尾，这样写个周期就可以被异步内存回收线程遍历到*/
			if(0 == is_writeonly_file)
				list_add(&p_file_stat_base->hot_cold_file_list,&hot_cold_file_global_info.file_stat_small_file_head);
			else
				list_add_tail(&p_file_stat_base->hot_cold_file_list,&hot_cold_file_global_info.file_stat_small_file_head);
		}
		else{
			hot_cold_file_global_info.mmap_file_stat_count++;
			list_add(&p_file_stat_base->hot_cold_file_list,&hot_cold_file_global_info.mmap_file_stat_small_file_head);
		}
		spin_unlock(p_global_lock);
	}
	else if(FILE_STAT_NORMAL == file_type){
		struct file_stat *p_file_stat;

		p_file_stat = kmem_cache_alloc(hot_cold_file_global_info.file_stat_cachep,GFP_ATOMIC);
		if (!p_file_stat) {
			spin_unlock(&hot_cold_file_global_info.mmap_file_global_lock);
			printk("%s file_stat alloc fail\n",__func__);
			goto out;
		}
		memset(p_file_stat,0,sizeof(struct file_stat));

		spin_lock(p_global_lock);

		if(IS_SUPPORT_FILE_AREA_READ_WRITE(mapping) && !free_old_file_stat){
			spin_unlock(p_global_lock);

			kmem_cache_free(hot_cold_file_global_info.file_stat_cachep,p_file_stat);
			p_file_stat_base = (struct file_stat_base *)mapping->rh_reserved1;

			printk("%s file_stat:0x%llx already alloc\n",__func__,(u64)mapping->rh_reserved1);
			goto out;  
		}

		p_file_stat_base = &p_file_stat->file_stat_base;
		file_stat_base_init(mapping,p_file_stat_base,is_cache_file);

		//设置文件是mmap文件状态，有些mmap文件可能还会被读写，要与cache文件互斥，要么是cache文件要么是mmap文件，不能两者都是 
		//set_file_stat_in_mmap_file_base(p_file_stat_base);
		INIT_LIST_HEAD(&p_file_stat->file_area_hot);
		INIT_LIST_HEAD(&p_file_stat->file_area_warm);
		INIT_LIST_HEAD(&p_file_stat->file_area_warm_hot);
		INIT_LIST_HEAD(&p_file_stat->file_area_warm_cold);
		INIT_LIST_HEAD(&p_file_stat->file_area_writeonly_or_cold);
		
		INIT_LIST_HEAD(&p_file_stat_base->file_area_temp);
		INIT_LIST_HEAD(&p_file_stat->file_area_free);
		
		/*新分配的file_stat必须设置in_file_stat_temp_head_list链表。这个设置file_stat状态的操作必须放到 把file_stat添加到
		 *tmep链表前边，还要加内存屏障。否则会出现一种极端情况，异步内存回收线程从temp链表遍历到这个file_stat，
		 *但是file_stat还没有设置为in_temp_list状态。这样有问题会触发panic。因为mmap文件异步内存回收线程，
		 *从temp链表遍历file_stat没有mmap_file_global_lock加锁，所以与这里存在并发操作。而针对cache文件，异步内存回收线程
		 *从global temp链表遍历file_stat，全程global_lock加锁，不会跟向global temp链表添加file_stat存在方法，但最好改造一下*/
		set_file_stat_in_file_stat_temp_head_list_base(p_file_stat_base);
		smp_wmb();
		
		if(is_cache_file){
			hot_cold_file_global_info.file_stat_count++;
			/*writeonly文件要移动到链表尾，这样写个周期就可以被异步内存回收线程遍历到*/
			if(0 == is_writeonly_file)
				list_add(&p_file_stat_base->hot_cold_file_list,&hot_cold_file_global_info.file_stat_temp_head);
			else{
				/*得到temp文件的current_scan_file_stat_info*/
				struct current_scan_file_stat_info *p_current_scan_file_stat_info = get_normal_file_stat_current_scan_file_stat_info(&hot_cold_file_global_info,1 << F_file_stat_in_file_stat_temp_head_list,is_cache_file);
				/* 把新分配的file_stat移动到global->temp链表尾，而如果current_scan_file_stat_info[CURRENT_SCAN_FILE_STAT_INFO_TEMP]
				 * ->p_traverse_file_stat不是NULL，说明它保存了上个周期遍历的glboal->temp链表尾的file_stat->warm链表的
				 * file_area，但没有遍历完file_stat->warm链表上所有的file_area，于是p_traverse_file_stat指向该file_stat。
				 * 该p_traverse_file_stat永远指向global->temp链表尾的file_stat，这是规定。现在要向glboal->temp链表添加
				 * 新的file_stat了，必须令p_traverse_file_stat赋值NULL。下个周期，从global->temp链表尾得到这个新的
				 * file_stat，再赋值p_traverse_file_stat*/
				update_file_stat_next_multi_level_warm_or_writeonly_list(p_current_scan_file_stat_info,p_file_stat);         
				list_add_tail(&p_file_stat_base->hot_cold_file_list,&hot_cold_file_global_info.file_stat_temp_head);
			}
		}
		else{
			hot_cold_file_global_info.mmap_file_stat_count++;
			list_add(&p_file_stat_base->hot_cold_file_list,&hot_cold_file_global_info.mmap_file_stat_temp_head);
		}

		spin_unlock(p_global_lock);

	}else
		BUG();

	if(shrink_page_printk_open)
		printk("%s file_stat:0x%llx\n",__func__,(u64)p_file_stat_base);

out:
	return p_file_stat_base;
}

/* 引入global_file_stat，在把file_stat_tiny_small文件的file_area都移动到global_file_stat链表后，是令该文件的
 * mapping->rh_reserved1 = &p_hot_cold_file_global->global_file_stat.file_stat_base，即mapping->rh_reserved1 指向
 * global_file_stat.file_stat_base，后续就跟普通文件的file_stat_base一摸一样了，因此file_area_alloc_and_init()分配
 * file_area，初始化file_area，把file_area移动到p_file_stat_base->file_area_temp链表，对global_file_stat和普通的
 * 文件file_stat来说，处理都一样*/
inline static struct file_area *file_area_alloc_and_init(unsigned int area_index_for_page,struct file_stat_base *p_file_stat_base,struct address_space *mapping)
{
	struct file_area *p_file_area = NULL;

	/*大部分小于1M的so文件，最初都是read/write读写，被判定为cache文件。转成global_file_stat后，file_area都是移动到了
	 * hot_cold_file_global_info.global_file_stat链表。后续mmap建立映射后，检测到该情况，分配page和file_area，要令
	 * mapping->rh_reserved1 = hot_cold_file_global_info.global_mmap_file_stat.file_stat_base，使后续分配的file_area都移动到global_mmap_file_stat链表*/
	if(mapping_mapped(mapping) && file_stat_in_global_base(p_file_stat_base)){
		if(p_file_stat_base != &hot_cold_file_global_info.global_mmap_file_stat.file_stat.file_stat_base){
			printk("mapping:0x%llx file_stat_base:0x%llx change to global_mmap_file_stat\n",(u64)mapping,(u64)p_file_stat_base);
			mapping->rh_reserved1 = (u64)(&hot_cold_file_global_info.global_mmap_file_stat.file_stat.file_stat_base);
		}
	}

	/* 有个并发分配file_area造成file_area内存泄露的的bug，如果多进程分配同一个索引的file_area，在这里加锁阻塞。等第2加锁的进程加锁成功，
	 * 依然会分配同一个索引的file_area并返回，造成前边同一个索引的file_area泄露了。因此要加锁后判断同一个索引的file_area是否已经分配了
	 * 不用，因为执行该函数对同一个文件分配file_area前的add_folio函数，xas_lock_irq(&xas)加锁了，同一个文件同时只有一个函数执行该函数分配file_area*/
	spin_lock(&p_file_stat_base->file_stat_lock);
	
	/*到这里，针对当前page索引的file_area结构还没有分配,page_slot_in_tree是槽位地址，*page_slot_in_tree是槽位里的数据，就是file_area指针，
	  但是NULL，于是针对本次page索引，分配file_area结构*/
	p_file_area = kmem_cache_alloc(hot_cold_file_global_info.file_area_cachep,GFP_ATOMIC);
	if (!p_file_area) {
		printk("%s file_area alloc fail\n",__func__);
		goto out;
	}
	memset(p_file_area,0,sizeof(struct file_area));
	/* 新分配的file_area必须添加到file_stat->temp链表头，对于tiny small文件来说，保证in_refault、in_free、in_hot
	 * 的file_area一定聚聚在file_stat_tiny_small->temp链表尾，将来tiny small转换成small文件或者normal文件，
	 * 只用从file_stat_tiny_small->temp链表尾就能获取到in_refault、in_free、in_hot的file_area，然后移动到新的file_stat的对应链表*/
	list_add(&p_file_area->file_area_list,&p_file_stat_base->file_area_temp);
	//保存该file_area对应的起始page索引，一个file_area默认包含8个索引挨着依次增大page，start_index保存其中第一个page的索引
	p_file_area->start_index = area_index_for_page;//area_index_for_page * PAGE_COUNT_IN_AREA;
	
	p_file_stat_base->file_area_count ++;//文件file_stat的file_area个数加1
	set_file_area_in_temp_list(p_file_area);//新分配的file_area必须设置in_temp_list链表

	//在file_stat->file_area_temp链表的file_area个数加1
	p_file_stat_base->file_area_count_in_temp_list ++;
	set_file_area_in_init(p_file_area);
	p_file_area->mapping = mapping;

	
out:
	spin_unlock(&p_file_stat_base->file_stat_lock);

	/* 1:该文件可能被并发iput()释放掉  2:异步内存回收线程正把该文件并发由cache文件转成mmap文件 3:异步内存回收线程正把该文件并发由tiny small转成small或normal文件
	 * 还要考虑一点，如果异步内存回收线程，正在使用的file_stat正好是该p_file_stat_base，会有并发问题吗？想想不会的*/
	if(file_stat_in_file_stat_tiny_small_file_head_list_base(p_file_stat_base) && p_file_stat_base->file_area_count > NORMAL_TEMP_FILE_AREA_COUNT_LEVEL &&
			!file_stat_in_tiny_small_to_tail_base(p_file_stat_base)){
         
		/*设置in_tiny_small_to_tail标记，保证上边的if只成立一次*/
		set_file_stat_in_tiny_small_to_tail_base(p_file_stat_base);
		if(file_stat_in_cache_file_base(p_file_stat_base)){
			spin_lock(&hot_cold_file_global_info.global_lock);
			if( !file_stat_in_delete_base(p_file_stat_base) && file_stat_in_file_stat_tiny_small_file_head_list_base(p_file_stat_base) &&
					file_stat_in_cache_file_base(p_file_stat_base)){
				list_move_tail(&p_file_stat_base->hot_cold_file_list,&hot_cold_file_global_info.file_stat_tiny_small_file_head);
			}
			spin_unlock(&hot_cold_file_global_info.global_lock);
		}
		else{
			spin_lock(&hot_cold_file_global_info.mmap_file_global_lock);
			if(!file_stat_in_delete_base(p_file_stat_base) && file_stat_in_file_stat_tiny_small_file_head_list_base(p_file_stat_base)){
				list_move_tail(&p_file_stat_base->hot_cold_file_list,&hot_cold_file_global_info.mmap_file_stat_tiny_small_file_head);
			}
			spin_unlock(&hot_cold_file_global_info.mmap_file_global_lock);
		}

		hot_cold_file_global_info.file_stat_tiny_small_move_tail_count ++;
	}

	/* 新分配的file_area，不管有没有访问都赋值当前global_age，否则就是0，可能会被识别为冷file_area而迅速释放掉.因为怕有些
	 * file_area分配后，对应的page不会被读写，就没有机会执行到update函数，赋值file_area_age=global_age，并令access_count加1.
	 * 。决定这里不再赋值了，
	 * 而是读写时执行hot_file_update_file_status()再给file_area_age赋值。但是这里赋值的话，file_area_age跟global_age就相等了，
	 * 然后很快对应的page被访问了，而执行到update函数，if(file_area_age < global_age)不成立，就不会令file_area的access_count加
	 * 1。实际mysql测试，发现大量的文件，预读时分配folio和file_area。但是因为这里的赋值，导致这些file_area的page很快被访问时，
	 * 执行到update函数，if(file_area_age < global_age)不成立，就不会令file_area的access_count加1。然后异步内存回收线程扫描到
	 * 这些file_area，access_count是0，有很大概率被移动到下一层的warm链表，很容易被回收掉，然后发生refault。现在决定，这个赋值
	 * 还留着。update函数修改为if(file_area_age < global_age || test_and_clear_bit(F_file_area_in_init,&p_file_area->file_area_state))
	 * 这里新分配的file_area因为有init标记，如此该if依然成立，然后顺利令file_area的access_count加1*/
	p_file_area->file_area_age = hot_cold_file_global_info.global_age; 
	return p_file_area;
}
/*令inode引用计数减1，如果inode引用计数是0则释放inode结构*/
inline static void file_inode_unlock(struct file_stat_base * p_file_stat_base)
{
    struct inode *inode = p_file_stat_base->mapping->host;
    //令inode引用计数减1，如果inode引用计数是0则释放inode结构
	iput(inode);
}
inline static void file_inode_unlock_mapping(struct address_space *mapping)
{
    struct inode *inode = mapping->host;
    //令inode引用计数减1，如果inode引用计数是0则释放inode结构
	iput(inode);
}

/*对文件inode加锁，如果inode已经处于释放状态则返回0，此时不能再遍历该文件的inode的address_space的radix tree获取page，释放page，
 *此时inode已经要释放了，inode、address_space、radix tree都是无效内存。否则，令inode引用计数加1，然后其他进程就无法再释放这个
 *文件的inode，此时返回1。如果inode被iput释放而有free标记则返回-1。总之加锁失败返回值 <= 0*/
inline static int file_inode_lock(struct file_stat_base *p_file_stat_base)
{
	struct inode *inode;
	int lock_fail = 0;

	rcu_read_lock();
	smp_rmb();
	if(file_stat_in_delete_base(p_file_stat_base) || (NULL == READ_ONCE(p_file_stat_base->mapping))){
		//不要忘了异常return要先释放锁
		rcu_read_unlock();
		return 0;
	}

	/* 判断file_stat和file_stat->mapping->rh_reserved1是否匹配，不匹配则crash。注意，到这里rcu_read_lock可以确保该文件
	 * inode和mapping结构体不会被立即释放掉，可以放心使用file_stat->mapping->rh_reserved1*/
	is_file_stat_mapping_error(p_file_stat_base);

	inode = p_file_stat_base->mapping->host;

	spin_lock(&inode->i_lock);

	/*inode正被其他进程iput释放，加锁失败。此时该文件的file_stat就可能没有in_delete标记，这个遇到过*/
	if(inode->i_state & (I_FREEING|I_WILL_FREE|I_NEW)){
		lock_fail = 1;
	}
	/*如果inode引用计数是0了，说明没人再用，加锁失败。并且iput()强制触发释放掉该inode，否则会成为只有一个文件页的file_stat，
	 *但是又因加锁失败而无法回收，对内存回收干扰。但iput要放到spin_unlock(&inode->i_lock)后*/
	else if(atomic_read(&inode->i_count) == 0){
		if(!hlist_empty(&inode->i_dentry)){
			struct dentry *dentry = hlist_entry(inode->i_dentry.first, struct dentry, d_u.d_alias);
			if(dentry)
				printk("%s file_stat:0x%llx inode:0x%llx dentry:0x%llx %s icount0!!!!!!!\n",__func__,(u64)p_file_stat_base,(u64)inode,(u64)dentry,dentry->d_name.name);
			else 
				printk("%s file_stat:0x%llx inode:0x%llx dentry:0x%llx icount0!!!!!!!\n",__func__,(u64)p_file_stat_base,(u64)inode,(u64)dentry);
		}else{
			if(shrink_page_printk_open_important)
				printk("%s file_stat:0x%llx inode:0x%llx icount0!!!!!!! i_nlink:%d nrpages:%ld lru_list_empty:%d %s\n",__func__,(u64)p_file_stat_base,(u64)inode,inode->i_nlink,inode->i_mapping->nrpages,list_empty(&inode->i_lru),inode->i_sb->s_id);
		}
	}

	//加锁成功则令inode引用计数加1，之后就不用担心inode被其他进程释放掉
	if(0 == lock_fail)
		atomic_inc(&inode->i_count);

	spin_unlock(&inode->i_lock);
	rcu_read_unlock();

	    
	/*加锁成功lock_fail是0而返回1，因inode有free标记而加锁失败则lock_fail是1而返回-1*/
	return (0 == lock_fail) ? 1:-1;
}
inline static void file_area_access_count_clear(struct file_area *p_file_area)
{
}
inline static void file_area_access_count_add(struct file_area *p_file_area,int count)
{
}
inline static int file_area_access_count_get(struct file_area *p_file_area)
{
	return 0;
}

/*head代表一段链表，first~tail是这个链表尾的几个连续成员，该函数是把first~tail指向的几个成员移动到链表头*/
//void list_move_enhance(struct list_head *head,struct list_head *first,struct list_head *tail)
inline static void list_move_enhance(struct list_head *head,struct list_head *first)
{
	/*链表不能空*/
	if(!list_empty(head)){
		/*指向链表最后一个成员*/
		struct list_head *tail = head->prev;

		/*1:first不能指向链表头 2:first不能是链表的第一个成员 3:tail必须是链表尾的成员*/
		if(first != head && head->next != first && list_is_last(tail,head)){
			/*first的上一个链表成员*/
			struct list_head *new_tail = first->prev;
			/*链表的第一个成员*/
			struct list_head *old_head = head->next;

			/*head<-->old_head<-->new_tail<-->first<-->tail -----> head<-->first <-->tail<-->old_head<-->new_tail
			 *
			 *head<-->old_head(new_tail)<-->first(tail) -----> head<-->first(tail)<-->old_head(new_tail)
			 */
			head->next = first;
			head->prev = new_tail;

			first->prev = head;
			tail->next  = old_head;

			old_head->prev = tail;
			new_tail->next = head;

			if(shrink_page_printk_open_important)
				printk("%ps->list_move_enhance() head:0x%llx ok\n",__builtin_return_address(0),(u64)head);
		}else{
			if(shrink_page_printk_open_important || first != head->next)
			printk("%ps->list_move_enhance() head:0x%llx first:0x%llx head->next:0x%llx %d fail\n",__builtin_return_address(0),(u64)head,(u64)first,(u64)head->next,list_is_last(tail,head));
		}
	}
}
/*现在temp的file_area，是所有表示file_area所在链表的bit都是0，特殊处理*/
inline static int can_file_area_move_to_list_head_for_temp_list_file_area(struct file_area *p_file_area,struct list_head *file_area_list_head)
{
	//p_file_area在链表的后一个file_area
	struct file_area *p_file_area_next = list_next_entry(p_file_area, file_area_list);
	//p_file_area在链表的前一个file_area
	struct file_area *p_file_area_prev = list_prev_entry(p_file_area, file_area_list);

	/*file_area不能是链表头*/
	if(&p_file_area->file_area_list == file_area_list_head)
		return 0;
	
	/* 如果file_area不在file_area_in_list_type这个file_stat的链表上，测试失败
	 * 如果file_area检测到在其他file_stat链表上，测试失败
	 * */
	if(get_file_area_list_status(p_file_area) != 0 || get_file_area_list_status(p_file_area_prev) != 0 || get_file_area_list_status(p_file_area_next) != 0){
		printk_deferred("%ps->can_file_area_move file_area_list_head:0x%llx file_area:0x%llx state:0x%x next:0x%llx state:0x%x prev:0x%llx state:0x%x p_file_area_error\n",__builtin_return_address(0),(u64)file_area_list_head,(u64)p_file_area,p_file_area->file_area_state,(u64)p_file_area_next,p_file_area_next->file_area_state,(u64)p_file_area_prev,p_file_area_prev->file_area_state);
		return 0;
	}

	return 1;
}
/*测试file_area是否真的在file_area_in_list_type这个file_stat的链表(file_stat->temp、hot、refault、warm、mapcount链表)，不在则不能把p_file_area从链表尾的file_area移动到链表头*/
inline static int can_file_area_move_to_list_head(struct file_area *p_file_area,struct list_head *file_area_list_head,unsigned int file_area_in_list_type_bit)
{
	unsigned int file_area_in_list_type = 1 << file_area_in_list_type_bit;
	//p_file_area在链表的后一个file_area
	struct file_area *p_file_area_next = list_next_entry(p_file_area, file_area_list);
	//p_file_area在链表的前一个file_area
	struct file_area *p_file_area_prev = list_prev_entry(p_file_area, file_area_list);

	/*file_area不能是链表头*/
	if(&p_file_area->file_area_list == file_area_list_head)
		return 0;
	
	/* 如果file_area不在file_area_in_list_type这个file_stat的链表上，测试失败
	 * 如果file_area检测到在其他file_stat链表上，测试失败
	 * */
	if(0 == (p_file_area->file_area_state & file_area_in_list_type) ||  p_file_area->file_area_state & (~(file_area_in_list_type) & FILE_AREA_LIST_MASK)){ 
		MULTI_LEVEL_FILE_AREA_PRINTK("%ps->can_file_area_move file_area_list_head:0x%llx file_area:0x%llx state:0x%x file_area_in_list_type:0x%x p_file_area_error\n",__builtin_return_address(0),(u64)file_area_list_head,(u64)p_file_area,p_file_area->file_area_state,file_area_in_list_type);
		return 0;
	}

	/*同样检测前一个file_area是否合法*/
	/*这里有个问题，在遍历in_free链表上的file_area时，file_area有in_free和in_refault标记时，这里的prev file_area，下边的
	 * next file_area的异常前后file_area判定，有很大概率判定导致if成立，而直接return 0，导致无法把本次遍历的file_area移动到file_stat->free链表头*/
	if(&p_file_area_prev->file_area_list != file_area_list_head){
		if(0 == (p_file_area_prev->file_area_state & file_area_in_list_type) ||  p_file_area_prev->file_area_state & (~(file_area_in_list_type) & FILE_AREA_LIST_MASK)){
			MULTI_LEVEL_FILE_AREA_PRINTK("%ps->can_file_area_move file_area_list_head:0x%llx file_area:0x%llx p_file_area_prev:0x%llx state:0x%x file_area_in_list_type:0x%x p_file_area_prev error\n",__builtin_return_address(0),(u64)file_area_list_head,(u64)p_file_area,(u64)p_file_area_prev,p_file_area_prev->file_area_state,file_area_in_list_type);
			return 0;
		}
	}

	/*同样检测后一个file_area是否合法，但它可能是链表头，要过滤掉*/
	if(&p_file_area_next->file_area_list != file_area_list_head){
		if(0 == (p_file_area_next->file_area_state & file_area_in_list_type) ||  p_file_area_next->file_area_state & (~(file_area_in_list_type) & FILE_AREA_LIST_MASK)){
			MULTI_LEVEL_FILE_AREA_PRINTK("%ps->can_file_area_move file_area_list_head:0x%llx file_area:0x%llx p_file_area_next:0x%llx state:0x%x file_area_in_list_type:0x%x p_file_area_next error\n",__builtin_return_address(0),(u64)file_area_list_head,(u64)p_file_area,(u64)p_file_area_next,p_file_area_next->file_area_state,file_area_in_list_type);
			return 0;
		}
	}

	if(shrink_page_printk_open1)
		printk("%ps->can_file_area_move file_area_list_head:0x%llx file_area:0x%llx\n",__builtin_return_address(0),(u64)file_area_list_head,(u64)p_file_area);

	return 1;
}
inline static int can_file_area_move_to_list_head_for_small_file_other(struct file_area *p_file_area,struct list_head *file_area_list_head,unsigned int file_area_in_list_type)
{
	//p_file_area在链表的后一个file_area
	struct file_area *p_file_area_next = list_next_entry(p_file_area, file_area_list);
	//p_file_area在链表的前一个file_area
	struct file_area *p_file_area_prev = list_prev_entry(p_file_area, file_area_list);

	/*file_area不能是链表头*/
	if(&p_file_area->file_area_list == file_area_list_head)
		return 0;
	
	/* 如果file_area不在file_area_in_list_type这个file_stat的链表上，测试失败
	 * 如果file_area检测到在其他file_stat链表上，测试失败
	 * */
	if(0 == (p_file_area->file_area_state & file_area_in_list_type) ||  p_file_area->file_area_state & (~(file_area_in_list_type) & FILE_AREA_LIST_MASK)){ 
		MULTI_LEVEL_FILE_AREA_PRINTK("%ps->can_file_area_move other_list file_area_list_head:0x%llx file_area:0x%llx state:0x%x file_area_in_list_type:0x%x p_file_area_error\n",__builtin_return_address(0),(u64)file_area_list_head,(u64)p_file_area,p_file_area->file_area_state,file_area_in_list_type);
		return 0;
	}

	/*同样检测前一个file_area是否合法*/
	if(&p_file_area_prev->file_area_list != file_area_list_head){
		if(0 == (p_file_area_prev->file_area_state & file_area_in_list_type) ||  p_file_area_prev->file_area_state & (~(file_area_in_list_type) & FILE_AREA_LIST_MASK)){
			MULTI_LEVEL_FILE_AREA_PRINTK("%ps->can_file_area_move other_list file_area_list_head:0x%llx file_area:0x%llx p_file_area_prev:0x%llx state:0x%x file_area_in_list_type:0x%x p_file_area_prev error\n",__builtin_return_address(0),(u64)file_area_list_head,(u64)p_file_area,(u64)p_file_area_prev,p_file_area_prev->file_area_state,file_area_in_list_type);
			return 0;
		}
	}

	/*同样检测后一个file_area是否合法，但它可能是链表头，要过滤掉*/
	if(&p_file_area_next->file_area_list != file_area_list_head){
		if(0 == (p_file_area_next->file_area_state & file_area_in_list_type) ||  p_file_area_next->file_area_state & (~(file_area_in_list_type) & FILE_AREA_LIST_MASK)){
			MULTI_LEVEL_FILE_AREA_PRINTK("%ps->can_file_area_move other_list file_area_list_head:0x%llx file_area:0x%llx p_file_area_next:0x%llx state:0x%x file_area_in_list_type:0x%x p_file_area_next error\n",__builtin_return_address(0),(u64)file_area_list_head,(u64)p_file_area,(u64)p_file_area_next,p_file_area_next->file_area_state,file_area_in_list_type);
			return 0;
		}
	}

	if(shrink_page_printk_open1)
		printk("%ps->can_file_area_move other_list file_area_list_head:0x%llx file_area:0x%llx\n",__builtin_return_address(0),(u64)file_area_list_head,(u64)p_file_area);
	return 1;
}

inline static int can_file_stat_move_to_list_head_for_one(struct file_stat_base *p_file_stat_base,unsigned int file_stat_in_list_type,char is_cache_file)
{
	if(is_cache_file && !file_stat_in_cache_file_base(p_file_stat_base)){
		printk_deferred("%ps->can_file_stat_move one file_stat:0x%llx status:0x%x  file_stat_in_list_type_bit:%d mmap file not move to cache file head\n",__builtin_return_address(0),(u64)p_file_stat_base,p_file_stat_base->file_stat_status,file_stat_in_list_type);
		return 1;
	}
	else if(!is_cache_file && !file_stat_in_mmap_file_base(p_file_stat_base)){
		printk_deferred("%ps->can_file_stat_move one file_stat:0x%llx status:0x%x file_stat_in_list_type_bit:%d cache file not move to  mmap file head\n",__builtin_return_address(0),(u64)p_file_stat_base,p_file_stat_base->file_stat_status,file_stat_in_list_type);
		return 1;
	}

	/*if成立说明file_stat有异常属性，被标记delete了或者从cache文件转成mmap文件而被list_del了等等*/
	if(file_stat_status_invalid_check(p_file_stat_base->file_stat_status)){
		printk_deferred("%ps->can_file_stat_move one file_stat:0x%llx status:0x%x file_stat_in_list_type_bit:%d file_stat_status_invalid_check error\n",__builtin_return_address(0),(u64)p_file_stat_base,p_file_stat_base->file_stat_status,file_stat_in_list_type);
		return 1;
	}

	/*1:如果file_stat不在file_stat_in_list_type这个global链表上，测试失败
	 *2:如果file_stat检测到在file_stat_in_list_type除外的其他global链表上，测试失败*/
	if(0 == (p_file_stat_base->file_stat_status & (1 << file_stat_in_list_type))  ||  p_file_stat_base->file_stat_status & (~(1 << file_stat_in_list_type) & FILE_STAT_LIST_MASK)){
		/*如果是tiny small file转成了tiny small file one area，不再打印，这个很常见*/
		if(!(file_stat_in_file_stat_tiny_small_file_one_area_head_list_base(p_file_stat_base) && (F_file_stat_in_file_stat_tiny_small_file_head_list == file_stat_in_list_type)))
		    printk_deferred("%ps->can_file_stat_move one file_stat:0x%llx status:0x%x file_stat_in_list_type_bit:%d file_stat_type error\n",__builtin_return_address(0),(u64)p_file_stat_base,p_file_stat_base->file_stat_status,file_stat_in_list_type);

		return 1;
	}

	return 0;
}
/*测试file_stat是否真的在file_stat_in_list_type这个global链表上(global temp、middle、large、hot、mapcount链表)，不在则不能把p_file_stat到链表尾的file_stat移动到链表头*/
inline static int can_file_stat_move_to_list_head(struct list_head *file_stat_temp_head,struct file_stat_base *p_file_stat_base,unsigned int file_stat_in_list_type,char is_cache_file)
{
	struct file_stat_base *p_file_stat_base_next = list_next_entry(p_file_stat_base, hot_cold_file_list);
	struct file_stat_base *p_file_stat_base_prev = list_prev_entry(p_file_stat_base, hot_cold_file_list);

	/*如果file_stat在链表前后没有成员，失败。如果file_stat_base已经从链表删除，失败*/
	if(list_empty(&p_file_stat_base->hot_cold_file_list) || p_file_stat_base->hot_cold_file_list.next == LIST_POISON1 || p_file_stat_base->hot_cold_file_list.prev == LIST_POISON2){
		printk_deferred("%ps->can_file_stat_move file_stat:0x%llx  is_cache_file:%d file_stat_in_list_type_bit:%d error!!!!!!!!!!!!!!\n",__builtin_return_address(0),(u64)p_file_stat_base,is_cache_file,file_stat_in_list_type);
		return 0;
	}

	/*检测p_file_stat_base是否合法*/
	if(can_file_stat_move_to_list_head_for_one(p_file_stat_base,file_stat_in_list_type,is_cache_file)){
		printk_deferred("%ps->can_file_stat_move file_stat:0x%llx status:0x%x is_cache_file:%d file_stat_in_list_type_bit:%d error\n",__builtin_return_address(0),(u64)p_file_stat_base,p_file_stat_base->file_stat_status,is_cache_file,file_stat_in_list_type);
		return 0;
	}

	/*检测p_file_stat_base在链表的下一个成员是否合法。如果它是链表头，就不检测了*/
	if(file_stat_temp_head != &p_file_stat_base_next->hot_cold_file_list){
		if(can_file_stat_move_to_list_head_for_one(p_file_stat_base_next,file_stat_in_list_type,is_cache_file)){
			printk_deferred("%ps->can_file_stat_move file_stat:0x%llx file_stat_next:0x%llx status:0x%x is_cache_file:%d file_stat_in_list_type_bit:%d error!!!\n",__builtin_return_address(0),(u64)p_file_stat_base,(u64)p_file_stat_base_next,p_file_stat_base_next->file_stat_status,is_cache_file,file_stat_in_list_type);
			return 0;
		}
	}

	/*检测p_file_stat_base在链表的上一个成员是否合法。它不可能是链表头，如果链表只有一个成员，prev是链表头但上边的if就会跳过。
	 *错了，还有一种情况，链表头有很多成员，p_file_stat_base是第一个成员，此时p_file_stat_base_prev就是链表头，因此还是要判断
	 *p_file_stat_base_prev是否是链表头*/
	if(file_stat_temp_head != &p_file_stat_base_prev->hot_cold_file_list){
		if(can_file_stat_move_to_list_head_for_one(p_file_stat_base_prev,file_stat_in_list_type,is_cache_file)){
			printk_deferred("%ps->can_file_stat_move file_stat:0x%llx file_stat_prev:0x%llx status:0x%x is_cache_file:%d file_stat_in_list_type_bit:%d error!!!\n",__builtin_return_address(0),(u64)p_file_stat_base,(u64)p_file_stat_base_prev,p_file_stat_base_prev->file_stat_status,is_cache_file,file_stat_in_list_type);
			return 0;
		}
	}

	if(shrink_page_printk_open1)
		printk_deferred("can_file_stat_move_to_list_head file_stat:0x%llx\n",(u64)p_file_stat_base);
	return 1;
}
inline static void i_file_stat_small_callback(struct rcu_head *head)
{
	struct file_stat_base *p_file_stat_base = container_of(head, struct file_stat_base, i_rcu);
	struct file_stat_small *p_file_stat_small = container_of(p_file_stat_base, struct file_stat_small, file_stat_base);

	/*有必要在这里判断file_stat的temp、refault、hot、free、mapcount链表是否空，如果有残留file_area则panic。
	 * 防止can_tiny_small_file_change_to_small_normal_file()把tiny small转换成其他文件时，因代码有问题，导致没处理干净所有的file_area*/
	if(!list_empty(&p_file_stat_small->file_stat_base.file_area_temp) || !list_empty(&p_file_stat_small->file_area_other))
		panic("%s file_stat_small:0x%llx status:0x%llx  list nor empty\n",__func__,(u64)p_file_stat_small,(u64)p_file_stat_small->file_stat_base.file_stat_status);

	kmem_cache_free(hot_cold_file_global_info.file_stat_small_cachep,p_file_stat_small);
}
inline static void i_file_stat_tiny_small_callback(struct rcu_head *head)
{
	struct file_stat_base *p_file_stat_base = container_of(head, struct file_stat_base, i_rcu);
	struct file_stat_tiny_small *p_file_stat_tiny_small = container_of(p_file_stat_base, struct file_stat_tiny_small, file_stat_base);

	/*有必要在这里判断file_stat的temp、refault、hot、free、mapcount链表是否空，如果有残留file_area则panic。
	 * 防止can_small_file_change_to_normal_file()把tiny small转换成其他文件时，因代码有问题，导致没处理干净所有的file_area*/
	if(!list_empty(&p_file_stat_tiny_small->file_stat_base.file_area_temp))
		panic("%s file_stat_small:0x%llx status:0x%llx  list nor empty\n",__func__,(u64)p_file_stat_tiny_small,(u64)p_file_stat_tiny_small->file_stat_base.file_stat_status);

	kmem_cache_free(hot_cold_file_global_info.file_stat_tiny_small_cachep,p_file_stat_tiny_small);
}

inline static void set_file_area_in_deleted(struct file_area *p_file_area)
{
	p_file_area->file_area_state = -1;
}
inline static char file_area_in_deleted(struct file_area *p_file_area)
{
	return (p_file_area->file_area_state == -1);
}
inline static void set_file_area_in_mapping_delete(struct file_area *p_file_area)
{
	set_file_area_in_mapping_exit(p_file_area);
}
inline static char file_area_in_mapping_delete(struct file_area *p_file_area)
{
	/* 非global_file_stat的正常的file_stat的file_area也会在iput()时把p_file_area->mapping设置NULL，这会导致这些file_stat的file_area被误判
	 * 为有file_area_in_mapping_delete标记，造成误判，把把个file_area移动到global_file_stat_delete链表，大错特错，因此要去掉(p_file_area->mapping == NULL)*/
	//return file_area_in_mapping_exit(p_file_area) || (p_file_area->mapping == NULL);
	return file_area_in_mapping_exit(p_file_area);
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(6,1,0) || defined(CONFIG_ASYNC_MEMORY_RECLAIM_FEATURE)
#define folio_try_get_rcu folio_try_get
#endif
extern int reverse_other_file_area_list_common(struct hot_cold_file_global *p_hot_cold_file_global,struct file_stat_base *p_file_stat_base,struct file_area *p_file_area,unsigned int file_area_type,unsigned int file_type,struct list_head *file_area_list);

inline void hot_file_update_file_status(struct address_space *mapping,struct file_stat_base *p_file_stat_base,struct file_area *p_file_area,int read_or_write);
extern char *get_file_name_buf(char *file_name_path,struct file_stat_base *p_file_stat_base);
extern int get_file_name_match(struct file_stat_base *p_file_stat_base,char *file_name1,char *file_name2,char *file_name3); 
extern char *get_file_name_no_lock_from_mapping(struct address_space *mapping);
extern unsigned int cold_mmap_file_isolate_lru_pages_and_shrink(struct hot_cold_file_global *p_hot_cold_file_global,struct file_stat_base * p_file_stat_base,struct file_area *p_file_area,struct page *page_buf[],int cold_page_count);
extern unsigned long shrink_inactive_list_async(unsigned long nr_to_scan, struct lruvec *lruvec,struct hot_cold_file_global *p_hot_cold_file_global,int is_mmap_file, enum lru_list lru);
extern int walk_throuth_all_mmap_file_area(struct hot_cold_file_global *p_hot_cold_file_global);
extern unsigned int cold_file_stat_delete_all_file_area(struct hot_cold_file_global *p_hot_cold_file_global,struct file_stat_base *p_file_stat_base,unsigned int file_type,char is_cache_file);
extern int cold_file_stat_delete(struct hot_cold_file_global *p_hot_cold_file_global,struct file_stat_base *p_file_stat_base_del,unsigned int file_type);
extern int cold_file_area_delete(struct hot_cold_file_global *p_hot_cold_file_global,struct file_stat_base *p_file_stat_base,struct file_area *p_file_area);

extern void file_stat_temp_middle_large_file_change(struct hot_cold_file_global *p_hot_cold_file_global,struct file_stat_base *p_file_stat_base,unsigned int file_stat_list_type, unsigned int normal_file_type,char is_cache_file);
extern int mmap_file_area_cache_page_solve(struct hot_cold_file_global *p_hot_cold_file_global,struct file_stat_base *p_file_stat_base,struct list_head *file_area_have_cache_page_head,struct list_head *file_area_free_temp,unsigned int file_type);
extern int cache_file_area_mmap_page_solve(struct hot_cold_file_global *p_hot_cold_file_global,struct file_stat_base *p_file_stat_base,struct list_head *file_area_have_mmap_page_head,unsigned int file_type);
extern int check_file_stat_is_valid(struct file_stat_base *p_file_stat_base,unsigned int file_stat_list_type,char is_cache_file);
extern noinline int hot_cold_file_print_all_file_stat(struct hot_cold_file_global *p_hot_cold_file_global,struct seq_file *m,int is_proc_print,int print_file_stat_info_or_update_refault);
extern noinline void printk_shrink_param(struct hot_cold_file_global *p_hot_cold_file_global,struct seq_file *m,int is_proc_print);
extern int hot_cold_file_thread(void *p);
extern int async_memory_reclaim_main_thread(void *p);

void page_cache_delete_for_file_area(struct address_space *mapping,struct folio *folio, void *shadow);
void page_cache_delete_batch_for_file_area(struct address_space *mapping,struct folio_batch *fbatch);
bool filemap_range_has_page_for_file_area(struct address_space *mapping,loff_t start_byte, loff_t end_byte);
bool filemap_range_has_writeback_for_file_area(struct address_space *mapping,loff_t start_byte, loff_t end_byte);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0) || defined(CONFIG_ASYNC_MEMORY_RECLAIM_FEATURE)
void replace_page_cache_folio_for_file_area(struct folio *old, struct folio *new);
#else
void replace_page_cache_page_for_file_area(struct page *old, struct page *new);
#endif
noinline int __filemap_add_folio_for_file_area(struct address_space *mapping,struct folio *folio, pgoff_t index, gfp_t gfp, void **shadowp);
pgoff_t page_cache_next_miss_for_file_area(struct address_space *mapping,pgoff_t index, unsigned long max_scan);
pgoff_t page_cache_prev_miss_for_file_area(struct address_space *mapping,pgoff_t index, unsigned long max_scan);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0) || defined(CONFIG_ASYNC_MEMORY_RECLAIM_FEATURE)
void *filemap_get_entry_for_file_area(struct address_space *mapping, pgoff_t index);
#else
void *mapping_get_entry_for_file_area(struct address_space *mapping, pgoff_t index);
#endif
inline struct folio *find_get_entry_for_file_area(struct xa_state *xas, pgoff_t max,xa_mark_t mark,struct file_area **p_file_area,unsigned int *page_offset_in_file_area,struct address_space *mapping);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0) || defined(CONFIG_ASYNC_MEMORY_RECLAIM_FEATURE)
unsigned find_get_entries_for_file_area(struct address_space *mapping, pgoff_t *start,pgoff_t end, struct folio_batch *fbatch, pgoff_t *indices);
unsigned find_lock_entries_for_file_area(struct address_space *mapping, pgoff_t *start,pgoff_t end, struct folio_batch *fbatch, pgoff_t *indices);
unsigned filemap_get_folios_contig_for_file_area(struct address_space *mapping,pgoff_t *start, pgoff_t end, struct folio_batch *fbatch);
unsigned filemap_get_folios_tag_for_file_area(struct address_space *mapping, pgoff_t *start,pgoff_t end, xa_mark_t tag, struct folio_batch *fbatch);
unsigned filemap_get_folios_for_file_area(struct address_space *mapping, pgoff_t *start,pgoff_t end, struct folio_batch *fbatch);
#else
unsigned find_get_entries_for_file_area(struct address_space *mapping, pgoff_t start,pgoff_t end, struct folio_batch *fbatch, pgoff_t *indices);
unsigned find_lock_entries_for_file_area(struct address_space *mapping, pgoff_t start,pgoff_t end, struct folio_batch *fbatch, pgoff_t *indices);
unsigned find_get_pages_range_for_file_area(struct address_space *mapping, pgoff_t *start,pgoff_t end, unsigned int nr_pages,struct page **pages);
unsigned find_get_pages_contig_for_file_area(struct address_space *mapping, pgoff_t index,unsigned int nr_pages, struct page **pages);
unsigned find_get_pages_range_tag_for_file_area(struct address_space *mapping, pgoff_t *index,pgoff_t end, xa_mark_t tag, unsigned int nr_pages,struct page **pages);
#endif
void filemap_get_read_batch_for_file_area(struct address_space *mapping,pgoff_t index, pgoff_t max, struct folio_batch *fbatch);

#endif
