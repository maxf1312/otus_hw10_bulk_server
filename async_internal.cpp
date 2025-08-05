#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>

#include "async_internal.h"

namespace otus_hw9{
    using namespace std;
    bool CommandQueueMT::pop(ICommandPtr_t& cmd) 
    {
        lk_t lk(guard_mx_);
        return BaseCls_t::pop(cmd);
    }

    ICommandQueue&     CommandQueueMT::push(ICommandPtr_t cmd)
    {
        lk_t lk(guard_mx_);
        return BaseCls_t::push( std::move(cmd) );
    }
    
    ICommandQueue&     CommandQueueMT::reset()
    {
        lk_t lk(guard_mx_);
        return BaseCls_t::reset();
    }

    size_t   CommandQueueMT::size() const
    {
        lk_t lk(guard_mx_);
        return BaseCls_t::size();
    }

    std::ostream&   CommandQueueMT::print(std::ostream& os) const
    {
        lk_t lk(guard_mx_);
        return BaseCls_t::print(os);
    }
    class PackagedCommandDecorator : public CommandDecorator
    {
    public:
        PackagedCommandDecorator(ICommandPtr_t inner_cmd, ICommandContextPtr_t ctx) 
            : CommandDecorator(inner_cmd), ctx_(ctx) 
        {
        }

        virtual void execute(ICommandContext&) override 
        {
            (*wrapped_cmd_)(*ctx_);
        }
    private:
        ICommandContextPtr_t  ctx_;    
    };

    class QueueExecutorWithPackingDecorator   : public QueueExecutorDecorator
    {
    public:
        using BaseCls_t = QueueExecutorToFile;
        QueueExecutorWithPackingDecorator(IQueueExecutorPtr_t wrapee) : QueueExecutorDecorator(std::move(wrapee))
        {
        } 
        
        virtual void execute_from_array(ICommandQueue& q, ICommandContext& ctx,
                                        const ICommandPtrArray_t& commands, size_t pos, size_t cnt) override
        {
            if( pos + cnt > commands.size() )
                cnt =  commands.size() - pos;

            // DBG_TRACE( "execute_from_array", "this: " << this << " wrapee_: " << wrapee_.get() 
            //             << ", q: " << &q << ", q: [" << q << "]" 
            //             << ", ctx: " << &ctx << ", commands.size: " << commands.size() << ", pos: " << pos << ", cnt: " << cnt
            //             << ", commands: [" << commands << "]"
            //             << ", q.bulk_id_: " << q.bulk_id_  << ", ctx.bulk_id_: " << ctx.bulk_id_
            //         )

            if( q.bulk_id_ != ctx.bulk_id_ )
            {
                q.bulk_id_ = ctx.bulk_id_.load();
                ICommandContextPtr_t  sp_cmd_ctx = make_shared<ICommandContext>(ctx);
                // трансформация элементов массива в очередь с запаковкой в декоратор с контекстом и исполнителем
                std::transform(begin(commands) + pos, begin(commands) + pos + cnt, 
                            back_inserter(q), [&](auto p_cmd){
                                    return make_shared<PackagedCommandDecorator>(p_cmd, sp_cmd_ctx);
                    }       
                );
                // DBG_TRACE( "execute_from_array", "this: " << this << " wrapee_: " << wrapee_.get() 
                //             << ", q: " << &q << ", q (AFTER ADD Packaged): [" << q << "]"
                //         )
            }
            execute(q, ctx, cnt);
        }
    };
    
    ProcessorMT::ProcessorMT(IInputParserPtr_t parser, ICommandQueuePtr_t cmd_queue, IQueueExecutorPtr_t executor) :
        Processor(std::move(parser), std::move(cmd_queue), std::move(executor))
    {
    }

    QueueExecutorMT::QueueExecutorMT(size_t thread_count) : 
        QueueExecutorMulti((thread_count < 2 ? thread_count = 2 : thread_count) + 1,
            otus_hw9::create_command_queue(ICommandQueue::Type::qLog),
            otus_hw9::create_command_queue(ICommandQueue::Type::qFile)
        )
    {
        // DBG_TRACE( "QueueExecutorMT", "this: " << this << ", log_q: " << log_queue_.get() << ", file_q: " << file_queue_.get() )

        // 0th - log thread
        // 1 - file queue provider threads
        // 2..N - file consumer threads
        IQueueExecutorPtr_t sp_exec = make_shared<QueueExecutorWithPackingDecorator>(make_shared<QueueExecutorWithThread>());
        // DBG_TRACE( "QueueExecutorMT", "this: " << this << ", log_executor: " << sp_exec.get() )

        add_worker(sp_exec);
        --thread_count;
        
        sp_exec = make_shared<QueueExecutorToFileInitializer>(
                        make_shared<QueueExecutorWithPackingDecorator>(nullptr), 
                            otus_hw9::create_command_queue(ICommandQueue::Type::qFile), 
                            make_shared<QueueExecutor>()      
                    );
        // DBG_TRACE( "QueueExecutorMT", "this: " << this << ", file_pusher_executor: " << sp_exec.get() )
        add_worker( sp_exec );
        size_t i{}; 
        std::ignore = i;
        while(thread_count-- > 0)
        {
            sp_exec = make_shared<QueueExecutorWithPackingDecorator>(make_shared<QueueExecutorWithThread>());
            // DBG_TRACE( "QueueExecutorMT", "this: " << this << ", file_in_thread_executor[" << i++ << "]: " << sp_exec.get() )
            add_worker(sp_exec); 
        }
    }

    std::mutex QueueExecutorWithThread::cmd_wait_mx;
    std::condition_variable QueueExecutorWithThread::cmd_wait_cv;

    QueueExecutorWithThread::QueueExecutorWithThread() : stop_flag_(false), q_(nullptr)
    {
        // DBG_TRACE( "QueueExecutorWithThread", "this: " << this )
    }

    QueueExecutorWithThread::~QueueExecutorWithThread()
    {
        // DBG_TRACE( "~QueueExecutorWithThread", "this: " << this << ", work_thread_.joinable: " << work_thread_.joinable() )
        if( work_thread_.joinable() )
        {
            stop_flag_ = true;
            cmd_wait_cv.notify_all();
            work_thread_.join();
        }
    }
    
    /// @brief получает стратегии и контекст, пробуждает поток  
    void QueueExecutorWithThread::execute(ICommandQueue& q, ICommandContext& ctx, size_t cnt)
    {
        // DBG_TRACE( "execute", "this: " << this << ", q: " << &q << ", q:[" << q << "]" << ", ctx: " << &ctx << ", cnt: " << cnt )
        if( !work_thread_.joinable() )
        {
            ctx_.reset( new ICommandContext(ctx) );
            ctx_->bulk_size_ = std::max(cnt, ctx_->bulk_size_.load());
            q_ = &q; 
            work_thread_ = std::thread{&QueueExecutorWithThread::execute_q, this};
            // DBG_TRACE( "execute", "this: " << this << " | work_thread_: " << hex << work_thread_.get_id() << ", ctx_: " << ctx_.get() )
        }
        cmd_wait_cv.notify_one();
    }
    
    void QueueExecutorWithThread::execute_q()
    {
        bool q_empty_at_stop = false;
        while( !stop_flag_ || !q_empty_at_stop )
        {
            if( q_ && ctx_ )
            {
                if(!stop_flag_)
                {
                    std::unique_lock  lk(cmd_wait_mx); 
                    cmd_wait_cv.wait(lk, [&](){ return !q_->empty() || stop_flag_; } );
                }
                ICommandPtr_t cmd;
                if( q_->pop(cmd) )
                {
                    // DBG_TRACE( "execute_q", "this: " << this << ", q_: " << q_ << ", q_:[" << *q_ << "]"
                    //            << ", ctx_: " << ctx_.get() << ", cmd: " << cmd.get() )
                    (*cmd)(*ctx_);
                }

                if(stop_flag_ && q_->empty()) q_empty_at_stop = true;
            }
        }
    }           

    /// @brief Фабрика очереди команд
    /// @return Указатель на абстрактный интерфейс очереди команд 
    ICommandQueuePtr_t create_command_queue(ICommandQueue::Type)
    {
        return ICommandQueuePtr_t{ new CommandQueueMT };
    }

    /// @brief Фабрика исполнителя очереди команд
    /// @return Указатель на созданный интерфейс
    IQueueExecutorPtr_t create_queue_executor(Options const& options)
    {
        return  IQueueExecutorPtr_t{  new QueueExecutorMT(options.thread_count) };
    }

    /// @brief  Фабрика для процессора, сама по настройкам выбирает какой тип процессора создать
    /// @param options 
    /// @return Интерфейс созданного объекта  
    IProcessorPtr_t create_processor(Options const& options)
    {
        if( options.thread_count < 2 )
            return otus_hw7::create_processor(options);
        return IProcessorPtr_t(new ProcessorMT(create_parser(options), 
                                               otus_hw9::create_command_queue(ICommandQueue::Type::qInput),
                                               create_queue_executor(options)));
    }
}

