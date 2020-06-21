#pragma once

#include <boost/timer/timer.hpp>

namespace util {

namespace boost {

::boost::timer::cpu_times &operator+=(::boost::timer::cpu_times &lhs, const ::boost::timer::cpu_times &rhs) {
	lhs.system += rhs.system;
	lhs.user += rhs.user;
	lhs.wall += rhs.wall;

	return lhs;
}

::boost::timer::cpu_times operator-(const ::boost::timer::cpu_times &lhs, const ::boost::timer::cpu_times &rhs) {
	::boost::timer::cpu_times result;

	result.system = lhs.system - rhs.system;
	result.user = lhs.user - rhs.user;
	result.wall = lhs.wall - rhs.wall;

	return result;
}

} // namespace boost

} // namespace util