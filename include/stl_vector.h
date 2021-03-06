#ifndef __LAMB_STL_INTERNAL_VECTOR_H_
#define __LAMB_STL_INTERNAL_VECTOR_H_

template <class T, class Alloc = alloc>
class Vector {//primary template
public:
	typedef T 			value_type;
	typedef value_type* pointer;
	typedef value_type* iterator;
	typedef const value_type* const_iterator;
	typedef value_type& reference;
	typedef const value_type& const_reference;
	typedef size_t		size_type;
	typedef ptrdiff_t	difference_type;

protected:
	typedef simple_alloc<value_type,alloc> data_allocator;
	iterator start;
	iterator finish;
	iterator end_of_storage;
	void insert_aux(iterator position, const T& x){
		if (finish != end_of_storage){//如果还有空余的空间
			construct(finish, *(finish - 1));
			++finish;
			T x_copy = x;
			copy_backward(position, finish - 2, finish - 1);
			*position = x_copy;		
		}else{//需要开辟新的空间
			const size_type old_size = size();
			const size_type len = old_size != 0 ? 2 * old_size : 1;//初始化/扩大 size
			
			iterator new_start = data_allocator::allocate(len);
			iterator new_finish = new_start;
			try{
				new_finish = uninitialized_copy(start, position, new_start);//将前半片旧数据拷贝到新开辟的空间内
				construct(new_finish, x);//末尾元素被构造成x
				++new_finish;
				new_finish = uninitialized_copy(position, finish, new_finish);//将后半片旧数据拷贝到新开辟的空间内
			}catch(...){//catch all the exceptions
				destroy(new_start, new_finish);
				data_allocator::deallocate(new_start, len);
				throw;
			}
			
			destroy(begin(),end());
			deallocate();//销毁原本的空间
			
			start = new_start;
			finish = new_finish;
			end_of_storage = new_start + len;
		}
	}
	void deallocate(){
		if (start)
			data_allocator::deallocate(start,end_of_storage - start);
	}
	void fill_initialize(size_type n, const T& value){
		start = allocate_and_fill(n,value);
		finish = start + n;
		end_of_storage = finish;
	}

public:
	iterator begin(){ return start; }
	const_iterator begin() const { return start; }
	iterator end() { return finish; }
	const_iterator end() const { return finish; }
	size_type size() const { return size_type(end() - begin()); }
	size_type capacity() const {
		return size_type(end_of_storage - begin());
	}
	bool empty() const { return begin() == end();}
	reference operator[](size_type n){ return *(begin() + n); }
	Vector():start(0), finish(0), end_of_storage(0){}
	~Vector(){
		destroy(start,finish);
		deallocate();
	}
	void pop_back(){
		--finish;
		destroy(finish);
	}
	
	reference back(){
		return *(end() - 1);
	}
	const_reference back() const {
		return *(end() - 1);
	}
	
	reference front(){
		return *begin();
	}
	const_reference front() const {
		return *begin();
	}
	
	
	iterator erase(iterator first, iterator last){
		iterator i = copy(last, finish, first);//将后半片旧数据往前移
		destroy(i,finish);//销毁end()
		finish = finish - (last - first);//确定finish的新位置
		return first;
	}
	
	iterator erase(iterator position){
		if (position + 1 != end()){
			copy(position + 1, finish, position);//后续元素往前移动
		}
		--finish;//end()往前移，注意末尾是开区间，故销毁end()所在的元素
		destroy(finish);
		return position;
	}
	
	void clear(){
		erase(begin(),end());
	}
	void push_back(const T& x){
		if (finish != end_of_storage){
			construct(finish, x);
			++finish;
		}else
			insert_aux(end(), x);
	}

	void insert(iterator position, size_type n, const T& x){
		if (n != 0){
			if (size_type(end_of_storage - finish) >= n){
				T x_copy = x;
				const size_type elems_after = finish - position;
				iterator old_finish = finish;
				if (elems_after > n){
					uninitialized_copy(finish - n, finish, finish);
					finish += n;
					copy_backward(position, old_finish - n, old_finish);
					fill(position, position + n, x_copy);
				}else{
					uninitialized_fill_n(finish, n - elems_after, x_copy);
					finish += n - elems_after;
					uninitialized_copy(position, old_finish, finish);
					finish += elems_after;
					fill(position, old_finish, x_copy);
				}
			}else{
				const size_type old_size = size();
				const size_type len = old_size + max(old_size, n);
				iterator new_start = data_allocator::allocate(len);
				iterator new_finish = new_start;
				__STL_TRY{
					new_finish = uninitialized_copy(start, position, new_start);
					new_finish = uninitialized_fill_n(new_finish, n, x);
					new_finish = uninitialized_copy(position, finish, new_finish);
				}
				# ifdef __STL_USE_EXCEPTIONS
					catch(...){
						destroy(new_start, new_finish);
						data_allocator::deallocate(new_start, len);
						throw;
					}
				#endif
				destroy(start, finish);
				deallocate();
				start = new_start;
				finish = new_finish;
				end_of_storage = new_start + len;
			}
		}
	}
	
	#ifdef __STL_MEMBER_TEMPLATES
    template <class ForwardIterator>
    iterator allocate_and_copy(size_type n, ForwardIterator first, ForwardIterator last) {
		iterator result = data_allocator::allocate(n);
		__STL_TRY {
		  uninitialized_copy(first, last, result);
		  return result;
		}
		__STL_UNWIND(data_allocator::deallocate(result, n));
	}
	#else /* __STL_MEMBER_TEMPLATES */
	iterator allocate_and_copy(size_type n, const_iterator first, const_iterator last) {
		iterator result = data_allocator::allocate(n);
		__STL_TRY {
		  uninitialized_copy(first, last, result);
		  return result;
		}
		__STL_UNWIND(data_allocator::deallocate(result, n));
	}
	#endif /* __STL_MEMBER_TEMPLATES */

	Vector<T, Alloc>& operator=(const Vector<T, Alloc>& x) {
		if (&x != this) {
			if (x.size() > capacity()) {//目标size过大
				iterator tmp = allocate_and_copy(x.end() - x.begin(), x.begin(), x.end());
				destroy(start, finish);
				deallocate();
				start = tmp;
				end_of_storage = start + (x.end() - x.begin());
			} else if (size() >= x.size()) {
				iterator i = copy(x.begin(), x.end(), begin());
				destroy(i, finish);
			} else {
				copy(x.begin(), x.begin() + size(), start);//size之前的elements必然已经construct完毕
				/* 
				TODO: 此处用法存疑,因为x中size之后的元素尚未被construct,那么复制过来的时候应该也无需construct,直接调用copy即可
				无论x中元素是否为POD
				*/
				uninitialized_copy(x.begin() + size(), x.end(), finish);//size之后的elements尚未被construct
			}
			finish = start + x.size();
		}
		return *this;
	}
};

#endif