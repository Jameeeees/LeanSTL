#ifndef __LAMB_STL_INTERNAL_DEFAULT_ALLOC_H_
#define __LAMB_STL_INTERNAL_DEFAULT_ALLOC_H_


#include <new>		/* placement new */
#include <cstddef>	/* for ptrdiff_t, size_t */
#include <cstdlib>	/* exit() */
#include <climits>	/* UINT_MAX */
#include <iostream> /* cerr */


enum {__ALIGN = 8};	//С��������ϵ��߽�
enum {__MAX_BYTES = 128};	//С�����������
enum {__NFREELISTS = __MAX_BYTES / __ALIGN};	//free_list ����

template <bool threads,int inst>
class __default_alloc_template{

private:
	static size_t ROUND_UP(size_t bytes){
		LOG("ROUNDING UP...","",0);
		//~(__ALIGN - 1) == 0xfffffffffffffff8
		//��bytes������8�ı���
		return (((bytes) + __ALIGN - 1) & ~(__ALIGN - 1));
	}
	union obj {
		union obj* free_list_link;
		char client_data[1];
	};
	
	static obj* volatile free_list[__NFREELISTS];
	static size_t FREELIST_INDEX(size_t bytes){
		return (((bytes) + __ALIGN - 1) / __ALIGN - 1);
	}
	
	static void* refill(size_t n);
	static char* chunk_alloc(size_t size, int& nobjs);
	
	static char* start_free;	//�ڴ����ʼλ�ã�ֻ��chunk_alloc()�б仯
	static char* end_free;		//�ڴ�ؽ���λ�ã�ֻ��chunk_alloc()�б仯
	static size_t heap_size;
	
public:
	static void* allocate(size_t n){
		LOG("allocate from memory pool...","bytes",n);
		obj* volatile * my_free_list;
		obj* result;
		if (n > (size_t) __MAX_BYTES){//�������128bytes,��ֱ�ӵ���malloc,�ڴ����û��ô���chunk
			return malloc_alloc::allocate(n);
		}
		my_free_list = free_list + FREELIST_INDEX(n);//�ҵ���Ӧ��С��freelistλ��
		result = *my_free_list;
		if (result == 0){
			//û�п��õ�freelist������ڴ�����
			void* r = refill(ROUND_UP(n));
			return r;
		}
		*my_free_list = result -> free_list_link;//��result��freelist��ȡ��������result
		return result;
	}
	static void deallocate(void* p,size_t n){
		LOG("deallocate to memory pool...","bytes",n);
		obj* q = (obj*)p;
		obj* volatile *my_free_list;
		
		if (n > (size_t) __MAX_BYTES){//���̫��ֱ�ӵ���mallocȥ����
			malloc_alloc::deallocate(p,n);
			return;
		}
		my_free_list = free_list + FREELIST_INDEX(n);
		
		q->free_list_link = *my_free_list;//���ͷŵ��ڴ�����·ŵ�freelist��
		*my_free_list = q;
	}
	static void* reallocate(void* p, size_t old_sz, size_t new_sz);
};

template <bool threads, int inst>
void* __default_alloc_template<threads, inst>::refill(size_t n){
	int nobjs = 20;//һ�η���20������
	char* chunk = chunk_alloc(n,nobjs);//pass by reference
	obj* volatile * my_free_list;
	obj* result;
	obj* current_obj, *next_obj;
	int i;
	LOG("refill from memory pool...","bytes",n);
	LOG("refill from memory pool...","chunks",nobjs);
	if (1 == nobjs){//���ֻ������һ�����飬��ֱ�Ӹ��ͻ�ʹ�ü���
		return chunk;
	}
	my_free_list = free_list + FREELIST_INDEX(n);
	result = (obj*)chunk;
	*my_free_list = next_obj = (obj*)(chunk + n);
	
	for (i = 1;;i++){//�����������������鶼���ӵ�freelist��
		current_obj = next_obj;
		next_obj = (obj*)((char*)next_obj + n);
		if (nobjs - 1 == i){
			current_obj -> free_list_link = 0;
			break;
		}else{
			current_obj -> free_list_link = next_obj;
		}
	}
	return result;
}

template <bool threads, int inst>
char* __default_alloc_template<threads,inst>::start_free = 0;

template <bool threads, int inst>
char* __default_alloc_template<threads,inst>::end_free = 0;

template <bool threads, int inst>
size_t __default_alloc_template<threads,inst>::heap_size = 0;

template <bool threads, int inst>
typename __default_alloc_template<threads, inst>::obj* volatile
__default_alloc_template<threads, inst>::free_list[__NFREELISTS] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

template <bool threads, int inst>
char* __default_alloc_template<threads, inst>::chunk_alloc(size_t size, int& nobjs){
	char* result;
	size_t total_bytes = size * nobjs;
	size_t bytes_left = end_free - start_free;//�ڴ��ʣ��ռ�
	
	if (bytes_left >= total_bytes){//����ڴ����ˮ������
		result = start_free;
		start_free += total_bytes;
		return result;
	}else if (bytes_left >= size){//����ڴ�������ٻ���һ��������ṩ
		nobjs = bytes_left / size;
		total_bytes = size * nobjs;
		result = start_free;
		start_free += total_bytes;
		return result;
	}else{//�����һ������Ҳ�ṩ����
		size_t bytes_to_get = 2 * total_bytes + ROUND_UP(heap_size >> 4);//�����ĵ�ǰ������ + ����ֵ
		if (bytes_left > 0){//�������ʣ����ڴ棬�������ã��������free_list
			obj* volatile * my_free_list = free_list + FREELIST_INDEX(bytes_left);
			((obj*)start_free) -> free_list_link = *my_free_list;
			*my_free_list = (obj*)start_free;
		}
		start_free = (char*)malloc(bytes_to_get);
		if (0 == start_free){//��heap�ռ䲻�㣬malloc����ʧ��
			int i;
			obj* volatile * my_free_list, *p;
			for (i = size; i <= __MAX_BYTES; i += __ALIGN){
				my_free_list = free_list + FREELIST_INDEX(i);
				p = *my_free_list;
				if (0 != p){
					*my_free_list = p->free_list_link;
					start_free = (char*)p;
					end_free = start_free + i;
					return chunk_alloc(size,nobjs);//�ݹ���������ڴ�
				}
			}
			end_free = 0;
			//ת�������ϲ��ڴ�������������oom���ƣ����׳��쳣
			start_free = (char*)malloc_alloc::allocate(bytes_to_get);
		}
		heap_size += bytes_to_get;
		end_free = start_free + bytes_to_get;
		return chunk_alloc(size,nobjs);//�ݹ���������ڴ�
	}
}


# ifdef __USE_MALLOC
typedef __malloc_alloc_template<0> malloc_alloc
typedef malloc_alloc alloc
# else
//typedef __default_alloc_template<__NODE_ALLOCATOR_THREADS,0> alloc;
typedef __default_alloc_template<0,0> alloc;
# endif



#endif






















