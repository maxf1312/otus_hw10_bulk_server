#pragma once

#ifndef __PRETTY_FUNCTION__
#include "pretty.h"
#endif

#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <ctime>

#ifdef USE_DBG_TRACE
#ifndef USE_PRETTY
#define DBG_TRACE( func, trace_statements )\
		std::cout << func << trace_statements << std::endl;
#else
#define DBG_TRACE( func, trace_statements )\
		do{\
			std::tm tm_buf{};\
			std::time_t t = std::time(nullptr);\
			std::ostringstream oss;\
			oss << std::put_time(localtime_r(&t, &tm_buf), "%F %T" ) << " | " << std::hex << std::this_thread::get_id() << " | "\
			          << __PRETTY_FUNCTION__ << " | " << std::dec <<  trace_statements << std::endl;\
			std::cout << oss.str();\
		}while(0);
#endif
#else
#define DBG_TRACE( func, trace_statements )   
#endif // USE_DBG_TRACE

