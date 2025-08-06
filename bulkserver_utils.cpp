#include <iostream>
#include "bulkserver_utils.h"

namespace otus_hw10{

    namespace{
        constexpr const char* const OPTION_NAME_PORT = "port";
        constexpr const char* const OPTION_NAME_CHUNK_SIZE = "chunk_size";  
    }

    Options& Options::add_caption_lines(std::string& caption)
    {
        BaseCls_t::add_caption_lines(caption);
        caption += "\nВызов: bulk_server <port> <chunk_size>";
        return *this;
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
    
    Options& Options::add_positional(otus_hw7::po::positional_options_description& pos_desc)
    {
        pos_desc.add(OPTION_NAME_PORT, 1); 
        pos_desc.add(OPTION_NAME_CHUNK_SIZE, -1);
        return *this;                        
    }   
};