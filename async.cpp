#include <iostream>
#include <unordered_set>

#include "async_internal.h"
#include "async.h"

using namespace std;

namespace otus_hw9{

    using IOStreamPtr_t = std::shared_ptr<std::iostream>;

    /// @brief Вспомогательный класс для хранения процессора и потока с данными 
    class LibAsyncCtx_t
    {
        constexpr static const size_t thread_cnt = 3;
    public:
        LibAsyncCtx_t(size_t bulk_size) : 
            iostream_(make_shared<stringstream>()), 
            processor_(create_processor(Options(bulk_size, iostream_.get(), thread_cnt)))
        {
        } 

        ~LibAsyncCtx_t() 
        {
        } 

        static mutex&     guard_mx()  { return guard_mx_; }
        IOStreamPtr_t&    iostream()  { return iostream_; } 
        IProcessorPtr_t&  processor() { return processor_; }

        void receive(const string_view& data, bool save_status_at_stop)
        {
            //append line to stream and run process
            if( !*iostream() )
                iostream()->clear();
                
            if( !data.empty() )
                *iostream() << data << std::endl; 
            processor()->process(save_status_at_stop);    
        }

    private:
        static mutex guard_mx_;
        IOStreamPtr_t iostream_;
        IProcessorPtr_t processor_;
    }; 

    using LibAsyncCtxPtr_t = shared_ptr<LibAsyncCtx_t>;
    using LibAsyncCtxPool_t = unordered_map<libasync_ctx_t, LibAsyncCtxPtr_t>;

    static LibAsyncCtxPool_t s_context_pool;

    mutex LibAsyncCtx_t::guard_mx_;
}


extern "C"
{
    libasync_ctx_t  connect(size_t bulk_size)
    {
        using namespace otus_hw9;
        std::ignore = bulk_size;

        unique_lock lk(LibAsyncCtx_t::guard_mx());        
        LibAsyncCtxPtr_t sp_async_ctx = make_shared<LibAsyncCtx_t>(bulk_size);
        s_context_pool[sp_async_ctx.get()] = sp_async_ctx;
        return sp_async_ctx.get();
    }

    int receive(libasync_ctx_t ctx, const char buf[], size_t buf_sz)
    {
        using namespace otus_hw9;
        std::ignore = ctx;
        std::ignore = buf;
        std::ignore = buf_sz;
        
        unique_lock lk(LibAsyncCtx_t::guard_mx());        

        auto p_ctx = s_context_pool.find(ctx);
        if( p_ctx == s_context_pool.end() )
            return -1;

        p_ctx->second->receive(std::string_view(buf, buf_sz), true);    
        return 0;
    }

    int disconnect(libasync_ctx_t ctx)
    {
        using namespace otus_hw9;
        std::ignore = ctx;

        unique_lock lk(LibAsyncCtx_t::guard_mx());        

        auto p_ctx = s_context_pool.find(ctx);
        if( p_ctx == s_context_pool.end() )
            return -1;
        
        p_ctx->second->receive(std::string_view(""), false);    
        //p_ctx->second->processor()->process(false);
        s_context_pool.erase(p_ctx);
        return 0;
    }
};

