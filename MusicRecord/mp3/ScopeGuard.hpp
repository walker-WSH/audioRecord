#pragma once

#define VS2013 1800
static_assert(_MSC_VER >= VS2013, "need c++11 support, at least vs2013");

#include <functional>
#include <typeinfo>
#include <tuple>

using namespace std;

//for global or static function
template<int N> class Execute {
public:
	template<typename F, typename T, typename... P> static void execute(F &&f, T &&t, P &&...p)
	{
		Execute<N - 1>::execute(f, std::forward<T>(t), std::get<N - 1>(std::forward<T>(t)), p...);
	}
};

template<> class Execute<0> {
public:
	template<typename F, typename T, typename... P> static void execute(F &&f, T &&t, P &&...p) { f(p...); }
};

template<typename F, typename... P> class ScopeGuard {
public:
	static ScopeGuard<F, P...> MakeScopeGurad(F f, P... p) { return ScopeGuard(std::move(f), std::move(p)...); }

	ScopeGuard(F &&f, P &&...p) throw() : function_(f), parameter_(p...), dismissed_(false){};

	~ScopeGuard()
	{
		if (!dismissed_) {
			try {
				Execute<sizeof...(P)>::execute(function_, parameter_);
			} catch (...) {
			}
		}
	};

	void Dismiss() const throw() { dismissed_ = true; }

private:
	F function_;
	tuple<P...> parameter_;
	mutable bool dismissed_;
};

template<typename F, typename... P> inline auto MakeGuard(F f, P... p) -> ScopeGuard<F, P...>
{
	return ScopeGuard<F, P...>::MakeScopeGurad(f, p...);
}

//for member function in class
template<int N> class ObjectExecute {
public:
	template<class O, typename F, typename T, typename... P> static void execute(O &&o, F &&f, T &&t, P &&...p)
	{
		ObjectExecute<N - 1>::execute(o, f, std::forward<T>(t), std::get<N - 1>(std::forward<T>(t)), p...);
	}
};

template<> class ObjectExecute<0> {
public:
	template<class O, typename F, typename T, typename... P> static void execute(O &&o, F &&f, T &&t, P &&...p) { (o.*f)(p...); }
};

template<class O, typename F, typename... P> class ObjectScopeGuard {
public:
	static ObjectScopeGuard<O, F, P...> MakeObjectScopeGurad(O &&o, F f, P... p) { return ObjectScopeGuard(std::move(o), std::move(f), std::move(p)...); }

	ObjectScopeGuard(O &&o, F &&f, P &&...p) throw() : object_(o), function_(f), parameter_(p...), dismissed_(false){};

	~ObjectScopeGuard()
	{
		if (!dismissed_) {
			try {
				ObjectExecute<sizeof...(P)>::execute(object_, function_, parameter_);
			} catch (...) {
			}
		}
	};

	void Dismiss() const throw() { dismissed_ = true; }

private:
	O &&object_;
	F function_;
	tuple<P...> parameter_;
	mutable bool dismissed_;
};

template<class O, typename F, typename... P> inline auto MakeObjectGurad(O &&o, F f, P... p) -> ObjectScopeGuard<O, F, P...>
{
	return ObjectScopeGuard<O, F, P...>::MakeObjectScopeGurad(std::move(o), f, p...);
}

#define CONCATENATE_DIRECT(s1, s2) s1##s2
#define CONCATENATE(s1, s2) CONCATENATE_DIRECT(s1, s2)
#define ANONYMOUS_VARIABLE(str) CONCATENATE(str, __LINE__)

#define ON_BLOCK_EXIT auto ANONYMOUS_VARIABLE(scopeGuard) = MakeGuard
#define ON_BLOCK_EXIT_OBJ auto ANONYMOUS_VARIABLE(scopeGuard) = MakeObjectGurad
