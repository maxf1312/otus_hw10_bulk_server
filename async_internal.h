#include <mutex>
#include <thread>
#include <condition_variable>

#include "bulk_internal.h"
#include "async_utils.h"

namespace otus_hw9{
    using std::istream;
    using std::ostream;

    using otus_hw7::IQueueExecutor;
    using otus_hw7::ICommandContext;
    using otus_hw7::IInputParser;
    using otus_hw7::ICommand;
    using otus_hw7::ICommandQueue;
    using otus_hw7::IProcessor;

    using otus_hw7::IQueueExecutorPtr_t;
    using otus_hw7::IInputParserPtr_t;
    using otus_hw7::ICommandPtr_t;
    using otus_hw7::ICommandQueuePtr_t;
    using otus_hw7::IProcessorPtr_t;
    using otus_hw7::ICommandContextPtr_t;

    using otus_hw7::ICommand;
    using otus_hw7::CommandDecorator;    
    using otus_hw7::CommandQueue;
    using otus_hw7::ICommandCreator;
    using otus_hw7::ICommandCreatorPtr_t;
    using otus_hw7::Processor;
    using otus_hw7::ICommandPtrArray_t;
    using otus_hw7::QueueExecutorToFile;
    using otus_hw7::QueueExecutorDecorator;
    using otus_hw7::QueueExecutorMulti;
    using otus_hw7::QueueExecutorToFileInitializer;
    using otus_hw7::QueueExecutorToBulkInitializer;

    /// @brief Реализация многопоточной очереди команд
    class CommandQueueMT : public CommandQueue
    {
    public:
        using BaseCls_t = CommandQueue;
        ICommandQueue&  push(ICommandPtr_t cmd) override;
        bool            pop(ICommandPtr_t& cmd) override;
        ICommandQueue&  reset() override;
        size_t          size() const override;
        std::ostream&   print(std::ostream& os) const override;

    private:
        using lk_t = std::unique_lock<std::mutex>;
        mutable std::mutex guard_mx_;
    };
   
    /// @brief Реализация исполнителя очереди для диспетчеризации по воркерам 
    class QueueExecutorMT : public QueueExecutorMulti
    {
    public:
        using BaseCls_t = QueueExecutorMulti;    
        QueueExecutorMT(size_t thread_count = 3);
    };

    /// @brief Реализация исполнителя очереди в отдельном потоке
    class QueueExecutorWithThread : public IQueueExecutor
    {
    public:    
        QueueExecutorWithThread();
        virtual ~QueueExecutorWithThread() override;
        virtual void execute(ICommandQueue& q, ICommandContext& ctx, size_t cnt) override;
    protected:
        void  execute_q();    
        bool  stop_flag_;
        ICommandQueue* q_;
        ICommandContextPtr_t ctx_;
        std::thread work_thread_;
        static std::mutex cmd_wait_mx;
        static std::condition_variable cmd_wait_cv;
    };

    /// @brief Реализация процессора команд
    class ProcessorMT : public Processor
    {
    public:
        using BaseCls_t = Processor;
        ProcessorMT(IInputParserPtr_t parser, ICommandQueuePtr_t cmd_queue, IQueueExecutorPtr_t executor);
    };

    /// @brief Фабрика очереди команд
    /// @return Указатель на абстрактный интерфейс очереди команд 
    ICommandQueuePtr_t create_command_queue(ICommandQueue::Type q_type);

    struct Options;
    /// @brief Фабрика исполнителя очереди команд
    /// @return Указатель на созданный интерфейс
    IQueueExecutorPtr_t create_queue_executor(Options const& options);

    /// @brief  Фабрика для процессора, сама по настройкам выбирает какой тип процессора создать
    /// @param options 
    /// @return Интерфейс созданного объекта  
    IProcessorPtr_t create_processor(Options const& options);
}