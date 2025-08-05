#include <iostream>
#include <fstream>
#include <sstream>

#include <map>

#include "bulk_internal.h"
#include "bulk_utils.h"


namespace otus_hw7{
    using namespace std;

    IInputParser::Status   InputParser::read_next_command(ICommandPtr_t& cmd)
    {
        read_command();
        command_data_t cmd_data;
        Status st = get_last_command_data(cmd_data);
        cmd = cmd_creator_->create_command(cmd_data, last_bulk_id_);
        return st;
    }

    IInputParser::Status   InputParser::read_next_bulk(ICommandQueue& cmd_queue)
    {
		static std::atomic<ICommandQueue::id_t> bulk_id{};
        size_t bulk_size = (save_status_at_stop_ && last_stat_ != Status::kReady) || !cmd_queue.empty() ? cmd_queue.bulk_size_.load() : 0;
        Status st{};
        if( cmd_queue.empty() )
            cmd_queue.created_at_ = std::time(nullptr),
            cmd_queue.bulk_id_ = (last_bulk_id_ = ++bulk_id); 

        // std::cout << hex << this_thread::get_id() << " | " 
        //            << "read_next_bulk() ENTRY, cmd_queue.bulk_size_: " << cmd_queue.bulk_size_.load() 
        //            << ", bulk_size: " << bulk_size 
        //            << ", q: " << cmd_queue << std::endl; 

        for(bool end_of_work = false, need_push_cmd = false; !end_of_work; need_push_cmd = false)
        {
            ICommandPtr_t cmd;
			switch( st = read_next_command(cmd) )
			{
				default:
				case Status::kIgnore:
					break;
				case Status::kReading:
                    need_push_cmd = true;
                    cmd = cmd_creator_->create_command_decorator(cmd,
                                                cmd_queue.empty() ? ICommandCreator::CommandType::cmdFirst : ICommandCreator::CommandType::cmdSimple
                                        );
					break;
				case Status::kReady:
                    if( !bulk_size && cmd_queue.empty() )
                        break;
                    end_of_work = true;
                    if(!cmd_queue.empty())
                    {
                        cmd = cmd_creator_->create_command_decorator(cmd_creator_->create_command(command_data_t{}, last_bulk_id_), ICommandCreator::CommandType::cmdLast);
                        need_push_cmd = true;
                    }
                    break;
				case Status::kStop:
                    end_of_work = true;
					if( !save_status_at_stop_ )
                        cmd_queue.reset();
					break;
			}
            if(need_push_cmd)
        	    cmd_queue.push(std::move(cmd)), bulk_size++;
		}
        cmd_queue.bulk_size_ = bulk_size;
        return st;
    }    

    void     InputParser::set_status(Status new_st)
    {
        if( new_st == last_stat_ )
            return;

        switch(new_st)
        {
            default: return;
            case InputParser::Status::kIgnore:
            case InputParser::Status::kReading:
    			break;
	
            case InputParser::Status::kReady:
                cmd_count_ = 0;
				break;

            case InputParser::Status::kStop:
                if( !save_status_at_stop_ )
                {
                    cmd_count_ = 0;
                    block_count_ = 0;
                }
                break;            
        }
        last_stat_ = new_st;
    }

    istream& InputParser::read_command()
    {
        using token_map_t = std::map<std::string, Token>;
        static token_map_t tok_values = {{"{", Token::kBegin_Block}, {"}", Token::kEnd_Block}};

        if( ((!save_status_at_stop_ && cmd_count_ > 0 && Status::kStop == last_stat_) || cmd_count_ == chunk_size_) && !block_count_ )
        {
            // std::cout << hex << this_thread::get_id() << " | " 
            //           << "save_status_at_stop_: " << save_status_at_stop_ 
            //           << ", last_stat_: " << int(last_stat_) 
            //           << ", block_count_: " << block_count_ << ", cmd_count_: " << cmd_count_ 
            //           << std::endl; 
            last_tok_ = Token::kEnd_Block;
            set_status(Status::kReady);
            return is_;
        }

        std::string inp_str;
        if( !std::getline(is_, inp_str) )
        {
            // std::cout << hex << this_thread::get_id() << " | " << "!std::getline(is_, inp_str), last_stat_: " << int(last_stat_) << ", block_count_: " << block_count_ << std::endl; 
            last_tok_ = block_count_ ? Token::kEnd_Of_File : Token::kEnd_Block;
            set_status( save_status_at_stop_ || (block_count_ || (Status::kReady == last_stat_)) ? Status::kStop : Status::kReady);
        }
        else
        {
            last_tok_ = Token::kCommand; 
            auto const p_tok = tok_values.find(inp_str);
            if( p_tok != tok_values.end() )
                last_tok_ = p_tok->second;

            // std::cout << hex << this_thread::get_id() << " | " << "getline() OK, inp_str: " << '\'' << inp_str << '\'' << std::endl; 
                
            switch( last_tok_ )
            {
                default:
                case Token::kCommand:
                    ++cmd_count_;
                    last_cmd_ = inp_str;
                    set_status(Status::kReading);
                    break;

                case Token::kBegin_Block:
                    if( !block_count_++ )
                        set_status(Status::kReady);
                    else
                        set_status(Status::kIgnore);
                    break;

                case Token::kEnd_Block:
                    if( block_count_ > 0 )
                    {
                        if( !--block_count_ )
                            set_status(Status::kReady);
                        else
                            set_status(Status::kIgnore);
                    }
                    break;
            }
        }
        return is_;
    }

    bool     CommandQueue::pop(ICommandPtr_t& cmd) 
    {
        if( q_.empty() )
            return false;
        cmd = std::move(q_.front());
        q_.pop();
        return true;
    }
    
    std::ostream& CommandQueue::print(std::ostream& os) const
    {
        CommandPrintExplorer explorer(os);
        queue_t q_print = q_;
        for( size_t i = 0; !q_print.empty() ; q_print.pop() )
        {
            if(i++) os << ", ";
            q_print.front()->explore_me(explorer);
        }
        os << std::endl;
        return os;
    }

    void IQueueExecutor::execute_from_array(ICommandQueue& cmd_q, ICommandContext& ctx,
                                        const ICommandPtrArray_t& commands, size_t pos, size_t cnt) 
    {
        cmd_q.copy_commands_from_array(commands, pos, cnt);
        execute(cmd_q, ctx);
    }      

    void QueueExecutor::execute(ICommandQueue& q, ICommandContext& ctx, size_t cnt)
    {
        cnt = std::min(cnt, q.size());
        DBG_TRACE( "execute", " this: " << this  
                    << ", &q: " << &q << ", cnt: " << cnt << ", q:[" << q << "]"  
                )
        ICommandPtr_t cmd;
        for( ; cnt-- > 0 && q.pop(cmd) ; ++ctx.cmd_idx_)
        { 
            (*cmd)(ctx);
        } 
    }


    void Processor::process(bool save_status_at_stop = false)
    {
		bool save_status_at_stop0 = parser_->save_status_at_stop(save_status_at_stop);
        for(bool end_of_work = false; !end_of_work;)
        {
			IInputParser::Status st = parser_->read_next_bulk(*cmd_queue_);
            // cout << hex << this_thread::get_id() << " | " << "read_next_bulk(), st: " << static_cast<uint16_t>(st) << endl; 
			switch( st )
			{
				default:
				case IInputParser::Status::kIgnore:
				case IInputParser::Status::kReading:
					break;
				case IInputParser::Status::kReady:
					exec_queue();
					break;
				case IInputParser::Status::kStop:
                    end_of_work = true;
					if( !save_status_at_stop ) 
                        cmd_queue_->reset();
					break;
			}
		}
		parser_->save_status_at_stop(save_status_at_stop0);
    }

    void    Processor::exec_queue( )
    {
        setup_context();
        ICommandContextPtr_t exec_ctx = std::make_shared<ICommandContext>(*ctx_);
        executor_->execute(*cmd_queue_, *exec_ctx, exec_ctx->bulk_size_);
    }

    void     Processor::setup_context()
    {
        ctx_->bulk_size_ = cmd_queue_->bulk_size_.load(); 
        ctx_->cmd_idx_ = 0;
        ctx_->cmd_created_at_ = cmd_queue_->created_at_;
        ctx_->bulk_id_ = cmd_queue_->bulk_id_.load();
    }


    QueueExecutorMulti::QueueExecutorMulti(size_t worker_count, ICommandQueuePtr_t log_queue, ICommandQueuePtr_t file_queue) : 
        max_workers_(worker_count), 
        log_queue_(log_queue), 
        file_queue_(file_queue)
    {
    }

    void QueueExecutorMulti::execute(ICommandQueue& q, ICommandContext& ctx, size_t cnt) 
    {
        // забираем из входной очереди bulk_size команд в массив  
        ICommandPtrArray_t commands{};
        q.move_commands_to_array(commands, cnt = std::min(ctx.bulk_size_.load(), cnt));

        // даем сигнал исполнителям выполнить из массива
        size_t i = 0;
        for( auto& worker_executor: workers_ )
        {
            worker_executor->execute_from_array(i ? *file_queue_ : *log_queue_, ctx, commands, 0, cnt);
            ++i;
        }        
    }

    QueueExecutorMulti& QueueExecutorMulti::remove_worker_at(size_t i)
    {  
        auto& w = workers_.at(i);
        auto p = std::remove(begin(workers_), end(workers_), w);
        workers_.erase(p, end(workers_));
        return *this;
    }
    
    void QueueExecutorMulti::check_worker_count()
    {
        if( workers_.size() >= max_workers_ )
            throw std::range_error("Max worker count was achived!");
    }

    /// @brief Фабрика для парсера. Опции нужны для выбора типа парсера
    /// @param options 
    /// @return 
    IInputParserPtr_t create_parser(Options const& options)
    {
        return IInputParserPtr_t{ new InputParser(options.cmd_chunk_sz, options.is_ ? *options.is_ : std::cin, ICommandCreatorPtr_t(new CommandCreator)) };
    }
    
    /// @brief Фабрика очереди команд
    /// @return Указатель на абстрактный интерфейс очереди команд 
    ICommandQueuePtr_t create_command_queue(ICommandQueue::Type)
    {
        return ICommandQueuePtr_t{ new CommandQueue };
    }

    /// @brief Фабрика исполнителя очереди команд
    /// @return Указатель на созданный интерфейс
    IQueueExecutorPtr_t create_queue_executor()
    {
        constexpr const size_t max_workers = 2;
        auto multi_q_executor = std::make_unique<QueueExecutorMulti>(max_workers);
        for(size_t i = 0; i < max_workers; ++i)
            multi_q_executor->add_worker( !i ? IQueueExecutorPtr_t{std::make_unique<QueueExecutor>()} 
                                             : IQueueExecutorPtr_t{std::make_unique<QueueExecutorToFileInitializer>(std::make_unique<QueueExecutor>())} 
                                        );
                                        
        return multi_q_executor;  
    }

    /// @brief  Фабрика для процессора, сама по настройкам выбирает какой тип процессора создать
    /// @param options 
    /// @return Интерфейс созданного объекта  
    IProcessorPtr_t create_processor(Options const& options)
    {
        return IProcessorPtr_t{new Processor(create_parser(options), create_command_queue(ICommandQueue::Type::qInput), create_queue_executor() ) };
    }
    
};