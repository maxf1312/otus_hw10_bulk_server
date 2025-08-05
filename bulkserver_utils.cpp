#include <iostream>
#include "bulkserver_utils.h"

namespace otus_hw10{

    namespace{
        constexpr const char* const OPTION_NAME_PORT = "port"; 
    }

    Options::BaseCls_t& Options::add_options(otus_hw7::po::options_description& desc)
    {
        BaseCls_t::add_options(desc);
        auto check_size = [](const uint16_t& port) 
                          { 
                            if( port < 1 ) throw otus_hw7::po::invalid_option_value(OPTION_NAME_PORT); 
                          };
        desc.add_options()
            (OPTION_NAME_PORT, otus_hw7::po::value<uint16_t>(&port)->notifier(check_size), "Номер порта для подключения");
        return *this;
    }        
};