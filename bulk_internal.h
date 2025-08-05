#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>

#include <map>
#include <algorithm>

#include "bulk.h"

#include "mydbgtrace.h"

namespace otus_hw7{

    using command_data_t = std::string;

    /// @brief Абстрактная фабрика для команды
    struct ICommandCreator
    {
        using CommandType = ICommand::CommandType;
        virtual ~ICommandCreator() = default;
        virtual ICommandPtr_t create_command(const command_data_t& cmd_data, ICommandQueue::id_t bulk_id, CommandType type = CommandType::cmdSimple) const = 0;
        virtual ICommandPtr_t create_command_decorator(ICommandPtr_t wrapee, CommandType type) const = 0;
    };
    using ICommandCreatorPtr_t = std::unique_ptr<ICommandCreator>;

    /// @brief Фабрика очереди команд
    /// @return Указатель на абстрактный интерфейс очереди команд 
    ICommandQueuePtr_t create_command_queue(ICommandQueue::Type);

    /// @brief Реализация парсера входного потока команд
    class InputParser : public IInputParser
    {
    public:
        InputParser(size_t chunk_size, istream& is, ICommandCreatorPtr_t cmd_creator) 
            : save_status_at_stop_(false), is_(is), chunk_size_(chunk_size), cmd_creator_{std::move(cmd_creator)},
              last_tok_{}, last_stat_{}, last_bulk_id_{} { }
        Status   read_next_command(ICommandPtr_t& cmd) override;        
        Status   read_next_bulk(ICommandQueue& cmd_queue) override;
        bool     save_status_at_stop(bool b_save) override 
        {
            std::swap(save_status_at_stop_, b_save);
            return b_save;
        }
        bool     save_status_at_stop() const override { return save_status_at_stop_; }

    private:
        enum class Token : uint8_t
        {
            kCommand,
            kBegin_Block,
            kEnd_Block,
            kEnd_Of_File
        };

        istream&   read_command();
        Status     get_last_command_data(command_data_t& cmd) const { cmd = last_cmd_; return last_stat_; }
        void       set_status(Status new_st);
        bool       save_status_at_stop_;
        istream&   is_;
        size_t     chunk_size_, cmd_count_ = 0, block_count_ = 0;

        ICommandCreatorPtr_t cmd_creator_;
        command_data_t  last_cmd_; 
        Token        last_tok_;       
        Status       last_stat_;
        ICommandQueue::id_t last_bulk_id_;               
    };

    class EmptyCommand;
    class CommandDecorator;
    class BulkCommand;
    struct ICommandVisitor
    {
        virtual void explore_cmd(ICommand const& cmd) = 0;        
        virtual void explore_cmd(EmptyCommand const& cmd) = 0;
        virtual void explore_cmd(CommandDecorator const& cmd_dec) = 0;
        virtual void explore_cmd(BulkCommand const& cmd_dec) = 0;
    };

    /// @brief Реализация пустой команды
    class EmptyCommand : public ICommand
    {
    public:
        EmptyCommand(const command_data_t& cmd, ICommandQueue::id_t bulk_id = ICommandQueue::id_t{}) : cmd_(cmd), bulk_id_{bulk_id} {}
        virtual void execute(ICommandContext&) override { }        
        virtual CommandType type() const override { return CommandType::cmdEmpty; }
        virtual ICommandQueue::id_t bulk_id() const override { return bulk_id_; }
        
        virtual void explore_me(ICommandVisitor& explorer) const override
        {
            explorer.explore_cmd(*this);
        }

        command_data_t cmd_data() const { return cmd_; }

    protected:
        command_data_t cmd_;
        ICommandQueue::id_t bulk_id_;
    };

    /// @brief Реализация простой команды, выводящей себя в поток
    class SimpleCommand : public EmptyCommand
    {
    public:
        SimpleCommand(const command_data_t& cmd, ICommandQueue::id_t bulk_id = ICommandQueue::id_t{}) : EmptyCommand(cmd, bulk_id) {}
        virtual void execute(ICommandContext& ctx) override
        {
            *ctx.os_ << cmd_; 
        }        
        virtual CommandType type() const override { return CommandType::cmdSimple; }
    };

  
    /// @brief Реализация команды-декоратора
    class CommandDecorator : public ICommand
    {
    public:
        CommandDecorator(ICommandPtr_t wraped_cmd) : wrapped_cmd_(std::move(wraped_cmd)) {}
        virtual void execute(ICommandContext& ctx) override 
        {
            if(wrapped_cmd_)    wrapped_cmd_->execute(ctx);
        }
        virtual CommandType type() const override
        {
            return wrapped_cmd_ ? wrapped_cmd_->type() : CommandType::cmdSimple;
        }
        virtual ICommandQueue::id_t bulk_id() const override
        {
            return wrapped_cmd_ ? wrapped_cmd_->bulk_id() : ICommandQueue::id_t{};
        }

        virtual void explore_me(ICommandVisitor& explorer) const override
        {
            explorer.explore_cmd(*this);
        }

        ICommandPtr_t wrapped_cmd() const { return wrapped_cmd_; }

    protected:
        ICommandPtr_t wrapped_cmd_;
    };

    /// @brief Реализация команды-декоратора для последней команды в блоке
    class SimpleCommandLast : public CommandDecorator
    {
    public:
        SimpleCommandLast(ICommandPtr_t wraped_cmd) : CommandDecorator(std::move(wraped_cmd)) {}
        virtual void execute(ICommandContext& ctx) override 
        {
            CommandDecorator::execute(ctx);
            *ctx.os_ << std::endl;
        }
        virtual CommandType type() const override
        {
            return CommandType::cmdLast;
        }
    };

    /// @brief Реализация декоратора команды для вывода в поток разделителя
    class SimpleCommandDelim : public CommandDecorator
    {
    public:
        using BaseCls_t = CommandDecorator;    
        SimpleCommandDelim(ICommandPtr_t wraped_cmd, const std::string& delim = ", " ) : BaseCls_t(std::move(wraped_cmd)), delim_(delim)  { }
        virtual void execute(ICommandContext& ctx) override 
        {
            *ctx.os_ << delim_; 
            CommandDecorator::execute(ctx);
        }
    private:
        std::string delim_;
    };

    /// @brief Реализация команды-декоратора для первой команды в блоке
    class SimpleCommandFirst : public SimpleCommandDelim
    {
    public:
        SimpleCommandFirst(ICommandPtr_t wraped_cmd) : SimpleCommandDelim(std::move(wraped_cmd), "bulk: ") {}
        virtual CommandType type() const override
        {
            return CommandType::cmdFirst;
        }
    };

    /// @brief Фабрика команд
    class CommandCreator : public ICommandCreator
    {
    public:
        virtual ICommandPtr_t create_command(const command_data_t&  cmd, ICommandQueue::id_t bulk_id, CommandType) const override 
        { 
            return std::make_shared<SimpleCommand>(cmd, bulk_id); 
        }

        virtual ICommandPtr_t create_command_decorator(ICommandPtr_t wrapee, CommandType type) const override
        {
            ICommandPtr_t p = std::move(wrapee);
            switch(type)
            {
                default: break;
                case CommandType::cmdSimple:
                    p.reset(new SimpleCommandDelim{std::move(p)});
                    break;
                case CommandType::cmdFirst:
                    p.reset(new SimpleCommandFirst{std::move(p)});
                    break;
                case CommandType::cmdLast:
                    p.reset(new SimpleCommandLast{std::move(p)});
                    break;
            }
            return p;
        }
    };

    /// @brief Реализация очереди команд
    class CommandQueue : public ICommandQueue
    {
    public:
        ICommandQueue&     push(ICommandPtr_t cmd) override { q_.push(std::move(cmd)); return *this; }
        bool               pop(ICommandPtr_t& cmd) override;
        ICommandQueue&     reset() override { while(!q_.empty()) q_.pop(); return *this; }
        size_t             size()  const override { return q_.size(); }
        bool               empty() const override { return q_.empty(); }

        ICommandQueue& move_commands_to_array(ICommandPtrArray_t& dst, size_t cnt) override
        {
            ICommandPtr_t cmd;
            while(cnt-- > 0 && pop(cmd))
                dst.push_back(std::move(cmd));    
            return *this;
        }

        ICommandQueue& copy_commands_from_array(ICommandPtrArray_t const& src, size_t pos, size_t cnt) override
        {
            for( auto p = src.begin() + pos, p_e = p + (pos + cnt > src.size() ? src.size() - pos : cnt); p != p_e; ++p)
                push(*p);
            return *this;
        }
        
        std::ostream& print(std::ostream& os) const override;

    protected:
        using  queue_t = std::queue<ICommandPtr_t>;
        queue_t q_;
    };

    inline std::ostream& operator<<(std::ostream& os, ICommandQueue& q){ return q.print(os); } 
    std::ostream& operator<<(std::ostream& os, ICommandPtrArray_t const& arr);
    
    /// @brief Реализация исполнителя очереди
    class QueueExecutor : public IQueueExecutor
    {
    public:    
        QueueExecutor()  { }
        virtual void execute(ICommandQueue& q, ICommandContext& ctx, size_t cnt) override;
    };


    /// @brief Реализация исполнителя очереди для диспетчеризации по воркерам 
    class QueueExecutorMulti : public QueueExecutor
    {
    public:    
        QueueExecutorMulti(size_t worker_count = 3,
                           ICommandQueuePtr_t log_queue = create_command_queue(CommandQueue::Type::qLog), 
                           ICommandQueuePtr_t file_queue = create_command_queue(CommandQueue::Type::qFile));
        virtual void execute(ICommandQueue& q, ICommandContext& ctx, size_t cnt) override;

        QueueExecutorMulti& add_worker(IQueueExecutorPtr_t worker) { check_worker_count(); workers_.emplace_back(std::move(worker)); return *this; }
        QueueExecutorMulti& remove_worker_at(size_t i);
        size_t              worker_count() const { return workers_.size(); }
    protected:
        using workers_t = std::vector<IQueueExecutorPtr_t>; 
        void    check_worker_count();

        const size_t       max_workers_;  
        ICommandQueuePtr_t log_queue_;
        ICommandQueuePtr_t file_queue_;
        workers_t          workers_;
    };

    /// @brief Декоратор для исполнителя очереди
    class QueueExecutorDecorator : public IQueueExecutor
    {
    public:
        QueueExecutorDecorator(IQueueExecutorPtr_t wrapee) : wrapee_(std::move(wrapee)) 
        {
        }

        virtual void execute(ICommandQueue& q, ICommandContext& ctx, size_t cnt) override 
        {
            if(wrapee_)    wrapee_->execute(q, ctx, cnt);
        }

        virtual void execute_from_array(ICommandQueue& q, ICommandContext& ctx,
                                const ICommandPtrArray_t& commands, size_t pos, size_t cnt) override 
        {
            if(wrapee_)    wrapee_->execute_from_array(q, ctx, commands, pos, cnt) ;
        }

        virtual void on_end_bulk(ICommandQueue& q, ICommand& cmd, ICommandContext& ctx) override 
        {
            if(wrapee_)    wrapee_->on_end_bulk(q, cmd, ctx);
        }      

    protected:
        IQueueExecutorPtr_t wrapee_;
    };


    class CmdLogFileSetuper 
    {
    public:
        CmdLogFileSetuper() : ctx_(std::make_shared<ICommandContext>())
        {
        }

        ICommandContextPtr_t context() const { return ctx_; }

    protected:
        void setup_context(ICommandContext& ctx, ICommandQueue& q)
        {
            std::unique_lock lk(guard_mx);
            *ctx_ = ctx;
            init_log(ctx, q);
            ctx_->os_ = log_;
        }

        std::string get_log_filenm(ICommandContext const& ctx)
        {
            std::ostringstream oss;
            oss << ctx.cmd_created_at_ << "-" << ctx.bulk_id_ 
                << "-" << std::hex << std::this_thread::get_id() 
                << "-" << this << ".log";
            return oss.str();
        }

        void init_log(ICommandContext const& ctx, ICommandQueue& q)
        {
            // DBG_TRACE( "init_log", "this: " << this  
            //             << " ctx: " << &ctx << " ctx.os_: " << ctx.os_.get() 
            //             << " ctx.bulk_id_: " << ctx.bulk_id_ << ", q.bulk_id_: " << q.bulk_id_ 
            //         )
            if( !log_ || (log_->is_open() && ctx.bulk_id_ != q.bulk_id_) )
            {
                log_ = std::make_shared<std::ofstream>();
                std::string file_nm = get_log_filenm(ctx);
                log_->open(file_nm, std::ios_base::out | std::ios_base::ate );
            }
        }
        std::shared_ptr<std::ofstream>  log_;
        ICommandContextPtr_t ctx_;
        std::mutex guard_mx;
    };

    /// @brief Реализация команды-декоратора для последней команды в блоке, для уведомления 
    class CommandLastInBulk : public CommandDecorator
    {
    public:
        CommandLastInBulk(ICommandPtr_t wraped_cmd, ICommandQueuePtr_t q,  IQueueExecutorPtr_t executor) 
            : CommandDecorator(std::move(wraped_cmd)), executor_(executor), q_(q) {}

        virtual void execute(ICommandContext& ctx) override 
        {
            CommandDecorator::execute(ctx);
            ICommandQueuePtr_t sp_q = q_.lock();
            IQueueExecutorPtr_t sp_executor = executor_.lock();

            // DBG_TRACE( "execute", " this: " << this  
            //         << " sp_q: " << sp_q.get() << " sp_executor: " << sp_executor.get()) 
            if( sp_executor && sp_q ) sp_executor->on_end_bulk(*sp_q, *this, ctx);
        }
        virtual CommandType type() const override
        {
            return CommandType::cmdLast;
        }
    protected:
        IQueueExecutorWPtr_t executor_;
        ICommandQueueWPtr_t  q_;
    };

    class CommandToFileInitDecorator : public CommandDecorator, public CmdLogFileSetuper
    {
    public:
        using BaseCls_t = CommandDecorator;
        CommandToFileInitDecorator(ICommandPtr_t wrapee, ICommandQueue& q) : CommandDecorator(std::move(wrapee)), q_(q)  
        {
        }

        virtual void execute(ICommandContext& ctx) override 
        {
            // DBG_TRACE( "execute", " this: " << this << " wrapee_: " << wrapped_cmd_.get())

            setup_context(ctx, q_);
            {
                std::unique_lock<std::mutex> lk(guard_mx);
                ctx.os_ = log_;
            }
            BaseCls_t::execute(*ctx_);
        }
    protected:
        ICommandQueue& q_;
    };

    class QueueExecutorToFile : public QueueExecutorDecorator, protected CmdLogFileSetuper
    {
    public:
        using BaseCls_t = QueueExecutorDecorator;
        QueueExecutorToFile(IQueueExecutorPtr_t wrapee) : QueueExecutorDecorator(std::move(wrapee))  
        {
        }

        virtual void execute(ICommandQueue& q, ICommandContext& ctx, size_t cnt) override 
        {
            setup_context(ctx, q);
            BaseCls_t::execute(q, *ctx_, cnt);
        }

        virtual void execute_from_array(ICommandQueue& q, ICommandContext& ctx,
                                        const ICommandPtrArray_t& commands, size_t pos, size_t cnt) override
        {
            setup_context(ctx, q);
            BaseCls_t::execute_from_array(q, *ctx_, commands, pos, cnt);
        }
    };

    class BulkCommand : public EmptyCommand
    {
    public:
        BulkCommand(ICommandPtrArray_t const& commands, size_t pos, size_t cnt,
                    ICommandQueuePtr_t q = std::make_shared<CommandQueue>(),
                    IQueueExecutorPtr_t q_executor = std::make_unique<QueueExecutor>()) 
            : EmptyCommand(command_data_t{}), queue_(q), queue_executor_(q_executor),
              cnt_(pos + cnt > commands.size() ? cnt = (commands.size() - pos) : cnt )
            {
                // DBG_TRACE( "BulkCommand", " this: " << this  
                //         << ", pos: " << pos << ", cnt_: " << cnt_  
                //         << ", commands.size(): " << commands.size()
                //         << ", commands: " << commands  
                //         << ", queue_: " << queue_.get() 
                //         << ", queue_executor_: " << queue_executor_.get()
                //         << ", cnt: " << cnt) 
                if(cnt)
                {
                    queue_->copy_commands_from_array(commands, pos, cnt);
                    
                    // queue_->copy_commands_from_array(commands, pos, cnt - 1);
                    // queue_->push(std::make_shared<CommandLastInBulk>(commands[cnt-1], queue_, queue_executor_));
 
                    // DBG_TRACE( "BulkCommand", " this: " << this << " queue_: " << *queue_) 
               }
            }

        virtual void execute(ICommandContext& ctx) override
        {
            // DBG_TRACE( "execute", " this: " << this  
            // << " ctx: " << &ctx << " queue_: " << queue_.get() << " queue_->size(): " << queue_->size()
            // << ", cnt_: " << cnt_ << ", queue_: " << *queue_)
            queue_executor_->execute(*queue_, ctx, cnt_);
        }

        virtual CommandType type() const override { return CommandType::cmdBulk; }
        virtual void explore_me(ICommandVisitor& explorer) const override
        {
            explorer.explore_cmd(*this);
        }

        ostream& print(ostream& os) const 
        {
            os << "{BulkCommand" << ", this: " << this << ", cnt_: " << cnt_ << ", queue_executor_: " << queue_executor_.get() 
               << ", queue_: " << queue_.get() << " [" << *queue_ << "]}"
               << std::endl;    
            return os;
        }
    protected:
        ICommandQueuePtr_t  queue_;  
        IQueueExecutorPtr_t queue_executor_;
        size_t              cnt_;
    };

    /// @brief Посетитель для печати команд в поток
    struct CommandPrintExplorer : ICommandVisitor
    {
        ostream& os_;
        CommandPrintExplorer(ostream& os) : os_(os){}
        virtual void  explore_cmd(ICommand const& ){}        
        virtual void  explore_cmd(EmptyCommand const& cmd)
        {
            os_ << '\'' << cmd.cmd_data() << '\''; ;
        }        

        virtual void  explore_cmd(CommandDecorator const& cmd_dec)
        {
            ICommandPtr_t cmd = cmd_dec.wrapped_cmd();
            if( cmd )
                cmd->explore_me(*this);
        }        

        virtual void  explore_cmd(BulkCommand const& cmd)
        {
            cmd.print(os_);                
        }        
    };

    /**
         * @brief  Вывод массива указазетелей на команды в поток
         * 
         * @param os 
         * @param arr 
         * @return std::ostream& 
         */
    inline std::ostream& operator<<(std::ostream& os, ICommandPtrArray_t const& arr)
    {
        CommandPrintExplorer explorer(os);
        for( size_t i = 0; i < arr.size() ; ++i )
        {
            if(i) os << ", ";
            arr[i]->explore_me(explorer);
        }
        os << std::endl;
        return os;
    } 

    /// @brief Класс - декоратор Исполнителя очереди для упаковки массива команд в BulkCommand
    class QueueExecutorToBulkInitializer : public QueueExecutorDecorator
    {
    public:
        using BaseCls_t = QueueExecutorDecorator;
        QueueExecutorToBulkInitializer(IQueueExecutorPtr_t wrapee, 
                                       ICommandQueuePtr_t q = std::make_shared<CommandQueue>(),
                                       IQueueExecutorPtr_t q_executor = std::make_unique<QueueExecutor>()) 
            : QueueExecutorDecorator(std::move(wrapee)), q_(q), q_executor_(q_executor)  
        {
        }

        virtual void execute_from_array(ICommandQueue& q, ICommandContext& ctx,
                                        const ICommandPtrArray_t& commands, size_t pos, size_t cnt) override
        {
            // DBG_TRACE( " execute_from_array", " this: " << this << " wrapee_: " << wrapee_.get()
            //            << ", &q: " << &q << ", q_executor_: " << q_executor_.get()
            //            << ", q:[" << q << "] "   
            //            << ", commands[" <<  commands << "], pos: " << pos << ", cnt: " << cnt
            // )

            ICommandPtrArray_t bulk_commands;
            if( !commands.empty() && cnt)
            {
                auto p_init_cmd = create_bulk_cmd(q, commands, pos, cnt);
                bulk_commands.emplace_back(p_init_cmd);
            }
            BaseCls_t::execute_from_array(q, ctx, bulk_commands, 0, 1);
        } 
    protected:

        /**
                 * @brief Create a bulk cmd object
                 * 
                 * @param commands массив команд
                 * @param pos с какой команды берем 
                 * @param cnt сколько
                 * @return  - указатель на созданную команду   
                 */
        virtual ICommandPtr_t create_bulk_cmd(ICommandQueue&, ICommandPtrArray_t const& commands, size_t pos, size_t cnt)
        {
            return std::make_shared<BulkCommand>(commands, pos, cnt, q_, q_executor_);
        }

        ICommandQueuePtr_t  q_;
        IQueueExecutorPtr_t q_executor_;
    };

    /**
         * @brief Класс - декоратор Исполнителя очереди для упаковки массива команд в блочную команду с инициализаций файла для вывода
         * 
         */
    class QueueExecutorToFileInitializer : public QueueExecutorToBulkInitializer
    {
    public:
        using BaseCls_t = QueueExecutorDecorator;
        QueueExecutorToFileInitializer(IQueueExecutorPtr_t wrapee, 
                                       ICommandQueuePtr_t q = std::make_shared<CommandQueue>(),
                                       IQueueExecutorPtr_t q_executor = std::make_unique<QueueExecutor>()) 
            : QueueExecutorToBulkInitializer(std::move(wrapee), q, q_executor)  
        {
        }

    protected:
        virtual ICommandPtr_t create_bulk_cmd(ICommandQueue& q_up, ICommandPtrArray_t const& commands, size_t pos, size_t cnt)
        {
            return std::make_shared<CommandToFileInitDecorator>(std::make_shared<BulkCommand>(commands, pos, cnt, q_, q_executor_), q_up);
        }
    };


    /// @brief Реализация процессора команд
    class Processor : public IProcessor
    {
    public:
        Processor(IInputParserPtr_t parser, ICommandQueuePtr_t cmd_queue, IQueueExecutorPtr_t executor) :
        parser_(std::move(parser)), cmd_queue_(std::move(cmd_queue)), executor_(std::move(executor)),
        ctx_(std::make_unique<ICommandContext>(0, 0, std::cout, 0)) {}
        void process(bool save_status_at_stop) override;
    
    protected:
        virtual void     exec_queue( );
        virtual void     setup_context();
        
        IInputParserPtr_t parser_;
        ICommandQueuePtr_t cmd_queue_; 
        IQueueExecutorPtr_t executor_;
        ICommandContextPtr_t ctx_;
    };

    /// @brief Фабрика для парсера. Опции нужны для выбора типа парсера
    /// @param options 
    /// @return 
    IInputParserPtr_t create_parser(Options const& options);

    /// @brief Фабрика исполнителя очереди команд
    /// @return Указатель на созданный интерфейс
    IQueueExecutorPtr_t create_queue_executor();

    /// @brief  Фабрика для процессора, сама по настройкам выбирает какой тип процессора создать
    /// @param options 
    /// @return Интерфейс созданного объекта  
    IProcessorPtr_t create_processor(Options const& options);
   
}