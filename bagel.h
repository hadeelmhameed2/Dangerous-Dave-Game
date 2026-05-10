/**
 * @file bagel.h
 * @brief Lightweight ECS helper templates used by the Dangerous Dave assignment.
 */

#pragma once
#include <cstdlib>
#include <algorithm>

namespace bagel
{
	using id_type = int;
	using ent_type = struct { id_type id; };

	struct NoInstance { 	NoInstance() = delete; };
	struct NoCopy {
		NoCopy() = default; // default constructor
		NoCopy(const NoCopy&) = delete;
		NoCopy& operator=(const NoCopy&) = delete;
	};

	template <class T, int N>
	class DynamicBag : NoCopy
	{
	public:
		int size() const { return _size; }
		void ensure(int new_capacity) {
			if (new_capacity > _capacity) {
				_capacity = std::max(_capacity*2, new_capacity);
				_arr = static_cast<T*>(
					realloc(_arr, sizeof(T)*_capacity));
			}
		}
		void push(const T& val) {
			if (_size == _capacity) {
				_capacity *= 2;
				_arr = static_cast<T*>(
					realloc(_arr, sizeof(T)*_capacity));
			}
			_arr[_size] = val;
			++_size;
		}
		T pop() {
			return _arr[--_size];
		}
		~DynamicBag() {
			free(_arr);
		}

		T& operator[](int idx) { return _arr[idx]; }
		const T& operator[](int idx) const { return _arr[idx]; }
	private:
		T* 		_arr = static_cast<T*>(
			malloc(sizeof(T) * N));
		int 		_size = 0;
		int 		_capacity = N;
	};

	template <class T>
	class SparseStorage final : NoInstance
	{
	public:
		static void add(ent_type ent, const T& val) {
			_comps.ensure(ent.id);
			_comps[ent.id] = val;
		}
		static T& get(ent_type ent) {
			return _comps[ent.id];
		}
	private:
		static inline DynamicBag<T,100> _comps;
	};
	template <class T>
	class TaggedStorage final : NoInstance
	{
	public:
		static void add(ent_type ent, const T& val) {
			//TODO: tag entity
		}
	private:
	};

	template <class T>
	class PackedStorage final : NoInstance
	{
	public:
		static void add(const ent_type ent, const T& val) {
			_idToComp.ensure(ent.id+1);
			_idToComp[ent.id] = _comps.size();
			_comps.push(val);
			_compToId.push(ent.id);
		}
		static void del(const ent_type ent) {
			int idx = _idToComp[ent.id];
			const id_type last = _compToId.pop();

			_comps[idx] = _comps.pop();
			_compToId[idx] = last;
			_idToComp[last] = idx;
		}
		static T& get(const ent_type ent) {
			return _comps[_idToComp[ent.id]];
		}
	private:
		static inline DynamicBag<T,100> _comps;
		static inline DynamicBag<int,100> _idToComp;
		static inline DynamicBag<id_type,100> _compToId;
	};
	template <class T>
	class StackStorage final : NoInstance
	{
	public:
		static void add(const ent_type ent, const T& val) {
			_idToComp.ensure(ent.id+1);
			if (_freeIdx.size() > 0) {
				const int idx = _freeIdx.pop();
				_idToComp[ent.id] = idx;
				_comps[idx] = val;
			}
			else {
				_idToComp.ensure(ent.id+1);
				_idToComp[ent.id] = _comps.size();
				_comps.push(val);
			}
			//TODO: remember empty/full cells
		}
		static void del(const ent_type ent) {
			_freeIdx.push(_idToComp[ent.id]);
		}
		static T& get(const ent_type ent) {
			return _comps[_idToComp[ent.id]];
		}
	private:
		static inline DynamicBag<T,100> _comps;
		static inline DynamicBag<int,100> _idToComp;
		static inline DynamicBag<id_type,100> _freeIdx;
	};

	template <class T>
	struct Storage final : NoInstance {
		using type = SparseStorage<T>;
	};

	class World final : NoInstance
	{
	public:
		static ent_type createEntity() {
			if (_ids.size() > 0)
				return {_ids.pop()};
			return {++_maxId};
		}
		static void deleteEntity(ent_type ent) {
			_ids.push(ent.id);
			//TODO: delete components
		}
	private:
		static inline DynamicBag<id_type,100> _ids;
		static inline id_type _maxId = -1;
	};
}
