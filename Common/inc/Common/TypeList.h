// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

template<class... Elems>
struct TypeList;

template<class First, class... Rest>
struct TypeList<First, Rest...> {
	using head = First;
	using tail = TypeList<Rest...>;

	static constexpr std::size_t size = 1 + sizeof...(Rest);
};

template<>
struct TypeList<> {
	static constexpr std::size_t size = 0;
};

//====

template<class TListFirst, class... TListOthers>
struct TypeListConcatImpl;

template<class TList1, class TList2, class... TListOthers>
struct TypeListConcatImpl<TList1, TList2, TListOthers...> {
	using type = typename TypeListConcatImpl<typename TypeListConcatImpl<TList1, TList2>::type, TListOthers...>::type;
};

template<class First1, class... Rest1, class... Elem2>
struct TypeListConcatImpl<TypeList<First1, Rest1...>, TypeList<Elem2...>> {
	using type = TypeList<First1, Rest1..., Elem2...>;
};

template<class... Elem2>
struct TypeListConcatImpl<TypeList<>, TypeList<Elem2...>> {
	using type = TypeList<Elem2...>;
};

template<class... Elem>
struct TypeListConcatImpl<TypeList<Elem...>> {
	using type = TypeList<Elem...>;
};

template<class... Lists>
using TypeListConcat = typename TypeListConcatImpl<Lists...>::type;

//====

template<class TList, class T>
struct TypeListRemoveIfPresentImpl {
	using type = TypeListConcat<TypeList<typename TList::head>, typename TypeListRemoveIfPresentImpl<typename TList::tail, T>::type>;
};

template<class T, class... Rest>
struct TypeListRemoveIfPresentImpl<TypeList<T, Rest...>, T> {
	using type = typename TypeListRemoveIfPresentImpl<TypeList<Rest...>, T>::type;
};

template<class T>
struct TypeListRemoveIfPresentImpl<TypeList<>, T> {
	using type = TypeList<>;
};


template<class TList, class T>
using TypeListRemoveIfPresent = typename TypeListRemoveIfPresentImpl<TList, T>::type;

//====

template<class TList>
struct TypeListMakeSetImpl;

template<class First, class... Rest>
struct TypeListMakeSetImpl<TypeList<First, Rest...>> {
	using type = TypeListConcat<TypeList<First>, typename TypeListMakeSetImpl<TypeListRemoveIfPresent<TypeList<Rest...>, First>>::type>;
};

template<>
struct TypeListMakeSetImpl<TypeList<>> {
	using type = TypeList<>;
};

template<class TList>
using TypeListMakeSet = typename TypeListMakeSetImpl<TList>::type;
