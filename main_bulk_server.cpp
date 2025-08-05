#include <iostream>
#include <algorithm>

#include "vers.h"
#include "bulkserver_utils.h"
#include "async.h"


using namespace std::literals::string_literals;

int main(int argc, char const* argv[]) 
{
	using namespace otus_hw10;
	try
	{
		Options options;
		if (!options.parse_command_line(argc, argv))
			return 1;
		
		libasync_ctx_t ctx = connect(options.cmd_chunk_sz);
		if( !ctx )
			throw std::runtime_error("Cannot connect to libasync!");
		
		std::string s_inp;
		for( ; std::getline(std::cin, s_inp) ; )
		{
			receive(ctx, s_inp.c_str(), s_inp.length());
		}
		disconnect(ctx);
	}	
	catch(const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
	}
	return 0;
}
