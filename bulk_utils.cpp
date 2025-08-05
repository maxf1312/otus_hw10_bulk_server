#include <iostream>
#include "bulk_utils.h"

namespace otus_hw7{
    namespace po = boost::program_options;
   
    void print_help(po::options_description const& desc)
    {
        std::cout << desc << std::endl;
    }
    namespace {
        constexpr const char* const OPTION_NAME_HELP = "help"; 
        constexpr const char* const OPTION_NAME_CHUNK_SIZE = "chunk_size"; 
    };
    Options& Options::add_options(po::options_description& desc)
    {
        auto check_size = [](const size_t& sz) 
                          { 
                            if( sz < 1 ) throw po::invalid_option_value(OPTION_NAME_CHUNK_SIZE); 
                          };
        desc.add_options()
            (OPTION_NAME_HELP, po::bool_switch(&show_help), "Отображение справки")
            (OPTION_NAME_CHUNK_SIZE, po::value<size_t>(&cmd_chunk_sz)->notifier(check_size), "Размер блока команд");

        return *this;
    }

    bool Options::parse_command_line(int argc, const char* argv[])
    {
        *this = Options();
        Options& parsed_options = *this;
        
        po::options_description desc("Аргументы командной строки");
        add_options(desc);

        po::positional_options_description pos_desc;
        pos_desc.add(OPTION_NAME_CHUNK_SIZE, -1);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(pos_desc).run(), vm);
        po::notify(vm);

        size_t sz = vm.size();
        bool not_need_exit = true;
        if( sz < 2 || !vm.count(OPTION_NAME_CHUNK_SIZE) )
            parsed_options.show_help = true, 
            not_need_exit = false;
        
        if( parsed_options.show_help )
            print_help(desc);
        
        return not_need_exit;
    }
};