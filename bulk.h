#pragma once

#include <ctime>
#include <iostream>
#include <string>
#include <memory>
#include <queue>
#include <atomic>

namespace otus_hw7{
    using std::istream;
    using std::ostream;

    struct IQueueExecutor;
    struct ICommandContext;
    struct IInputParser;
    struct ICommand;
    struct ICommandQueue;
    struct ICommandVisitor;
    struct IProcessor;

    using IQueueExecutorPtr_t = std::shared_ptr<IQueueExecutor>;
    using IQueueExecutorWPtr_t = std::weak_ptr<IQueueExecutor>;
    using IInputParserPtr_t = std::unique_ptr<IInputParser>;
    using ICommandPtr_t = std::shared_ptr<ICommand>;
    using ICommandQueuePtr_t = std::shared_ptr<ICommandQueue>;
    using ICommandQueueWPtr_t = std::weak_ptr<ICommandQueue>;
    using IProcessorPtr_t = std::unique_ptr<IProcessor>;
    using ICommandContextPtr_t = std::shared_ptr<ICommandContext>;
    using OStreamPtr_t = std::shared_ptr<std::ostream>;

    using ICommandPtrArray_t = std::vector<ICommandPtr_t>;
    //---------------------------------------------------------------------------------------------------
    
    /// @brief  Парсер для четния, разбора ввода и формирования пакетов команд. 
    ///         Формирует пакеты, возвращая сразу данные в ICommandQueue 
    struct IInputParser
    {
        /// @brief Статус чтения ввода и готовности к выполнению
        enum class Status : uint8_t
        {
            kReading,
            kReady,
            kIgnore,
            kStop
        };
        virtual          ~IInputParser() = default;
        virtual Status   read_next_command(ICommandPtr_t& cmd) = 0;
        virtual Status   read_next_bulk(ICommandQueue& cmd_queue) = 0;
        virtual bool     save_status_at_stop(bool b_save) = 0;        
        virtual bool     save_status_at_stop() const = 0;        
    };

    /// @brief Очередь команд. Формируется парсером, затем выполняется исполнителем под управлением процессора.
    struct ICommandQueue
    {
        using value_type = ICommandPtr_t;
        using id_t = unsigned long;

        enum class Type : uint8_t {
            qInput,
            qLog,
            qFile
        };  

        time_t created_at_;
        std::atomic<id_t>   bulk_id_;
        std::atomic<size_t> bulk_size_;

        ICommandQueue& push_back(ICommandPtr_t cmd){ return push(cmd); }

                ICommandQueue() : created_at_(std::time(nullptr)), bulk_id_{}, bulk_size_{} {}        
        virtual  ~ICommandQueue() = default;
        virtual  ICommandQueue& push(ICommandPtr_t cmd) = 0;
        virtual  bool pop(ICommandPtr_t& cmd) = 0;
        virtual  ICommandQueue& reset() = 0;
        virtual  size_t size() const = 0;
        virtual  bool   empty() const = 0;
        virtual  ICommandQueue& move_commands_to_array(ICommandPtrArray_t& dst, size_t cnt = size_t(-1)) = 0;
        virtual  ICommandQueue& copy_commands_from_array(ICommandPtrArray_t const& src, size_t pos = 0, size_t cnt = size_t(-1)) = 0;

        virtual std::ostream& print(std::ostream& os) const = 0;

    };

    /// @brief Команда, активный объект, паттерн команда
    struct ICommand
    {
        /// @brief Тип команды
        enum class CommandType : uint8_t 
        {
            cmdEmpty,  
            cmdSimple,  
            cmdFirst,
            cmdLast,
            cmdBulk
        };

        virtual      ~ICommand() = default;

        /// @brief Выполнить команду в заданном контексте
        virtual void execute(ICommandContext& ctx) = 0;
        
        /// @brief Обертка для превращения в Callable
        /// @param ctx -контекст команды 
        void operator()(ICommandContext& ctx){ execute(ctx); }
        
        /// @brief Тип команды
        virtual CommandType type() const = 0;

        /// @brief ИД блока, к которому принадлежит команда
        virtual ICommandQueue::id_t bulk_id() const = 0;

        virtual void explore_me(ICommandVisitor& explorer) const = 0;
    };

    /// @brief Актор, выполняющий очередь
    struct IQueueExecutor
    {
        virtual      ~IQueueExecutor() = default;
        virtual void execute(ICommandQueue& cmd_q, ICommandContext& ctx, size_t cnt = size_t(-1)) = 0;
        virtual void execute_from_array(ICommandQueue& cmd_q, ICommandContext& ctx,
                                        const ICommandPtrArray_t& commands, size_t pos = 0, size_t cnt = size_t(-1));
        virtual void on_end_bulk(ICommandQueue&, ICommand&, ICommandContext&){ }
    };

    /// @brief Контекст выполнения команды
    struct ICommandContext
    {
        std::atomic<size_t> bulk_size_;
        size_t cmd_idx_;
        OStreamPtr_t os_;
        time_t cmd_created_at_;
        std::atomic<ICommandQueue::id_t> bulk_id_;
        std::atomic_flag  interrupt_flag_; 

        virtual ~ICommandContext() = default; 
        ICommandContext() 
            : bulk_size_{}, cmd_idx_{}, os_{}, cmd_created_at_{std::time(nullptr)},
              bulk_id_{}, interrupt_flag_{false}  {} 
        ICommandContext(size_t bulk_size, size_t cmd_idx, ostream& os, time_t cmd_created_at) 
            : bulk_size_(bulk_size), cmd_idx_(cmd_idx), os_(&os, [](OStreamPtr_t::element_type*){;}), 
              cmd_created_at_(cmd_created_at), bulk_id_{}, interrupt_flag_(false) {}
        ICommandContext(size_t bulk_size, size_t cmd_idx, OStreamPtr_t os, time_t cmd_created_at) 
            : bulk_size_(bulk_size), cmd_idx_(cmd_idx), os_(os), 
              cmd_created_at_(cmd_created_at), bulk_id_{}, interrupt_flag_(false) {}
        ICommandContext(ICommandContext const& rhs) 
            : bulk_size_(rhs.bulk_size_.load()), cmd_idx_(rhs.cmd_idx_), os_(rhs.os_), 
              cmd_created_at_(rhs.cmd_created_at_), bulk_id_{rhs.bulk_id_.load()}, interrupt_flag_(false) {  }

        void swap(ICommandContext& rhs)
        {
            if( &rhs != this )
            {
                bulk_size_.exchange(rhs.bulk_size_);
                std::swap(cmd_idx_, rhs.cmd_idx_);
                std::swap(os_, rhs.os_);
                std::swap(cmd_created_at_, rhs.cmd_created_at_);
                bulk_id_.exchange(rhs.bulk_id_);
                bool lhs_f = interrupt_flag_.test_and_set();
                bool rhs_f = rhs.interrupt_flag_.test_and_set();
                std::swap(lhs_f, rhs_f);
                if( !lhs_f ) interrupt_flag_.clear();
                if( !rhs_f ) rhs.interrupt_flag_.clear();
            }
        } 

        ICommandContext& operator=(ICommandContext const& rhs)
        {
            if( &rhs != this )
            {
                ICommandContext tmp(rhs);
                swap(tmp);
            }
            return *this;
        } 
    };

    /// @brief Процессор - управляющий обработкой посредник
    struct IProcessor
    {
        virtual ~IProcessor() = default;
        virtual void process(bool save_status_at_stop = false) = 0;
    };

    
    struct Options;

    /// @brief  Фабрика для процессора, сама по настройкам выбирает какой тип процессора создать
    /// @param options 
    /// @return Интерфейс созданного объекта  
    IProcessorPtr_t create_processor(Options const& options);
} // otus_hw7

