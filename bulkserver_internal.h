#include <iostream>
#include <boost/asio.hpp>

#include "async.h"

namespace otus_hw10{
    namespace ba = boost::asio;
    using ba::ip::tcp;

    using std::istream;
    using std::ostream;

    /// @brief  Класс сессии приема и обработки команд. Для обработки устанавливает соединение с libasync и работает через него.
    ///         За основу взят класс session из примера Урок 31
    class async_session
    : public std::enable_shared_from_this<async_session>
    {
    public:
        async_session(tcp::socket socket, size_t bulk_size)
            : socket_(std::move(socket))
        {
            ctx_ = libasync_connect(bulk_size);
            if( !ctx_ )
            	throw std::runtime_error("Cannot connect to libasync!");

        }

        ~async_session()
        {
            libasync_disconnect(ctx_);
        }

        void start()
        {
            do_read();
        }

    private:
        /// @brief  Метод для запуска асинхронного чтения в буффер и вызова обработки.
        ///         Поскольку обработка не предполагает ответов - снова вызывается do_read() из обработчика 
        void do_read()
        {
            auto self(shared_from_this());
            socket_.async_read_some(ba::buffer(data_, max_length),
                [this, self](boost::system::error_code ec, std::size_t length)
                {
                    if (!ec)
                    {
                        //std::cout << "receive " << length << "=" << std::string{data_, length} << std::endl;
                        int rc = libasync_receive(ctx_, data_, length);
                        if( rc )
                            throw std::runtime_error("libasync_receive error: " + std::to_string(rc));
                        do_read();
                    }
                }
            );
        }

        tcp::socket socket_;
        enum { max_length = 1024 };
        char data_[max_length];
        libasync_ctx_t  ctx_;
    };


    /// @brief Сервер асинхронного приема соединений и их дальнейшей асинхронной обработки в объектах КлиентскаяСессия
    ///        За основу взят класс server из примера Урока 31
    class async_server
    {
    public:
        async_server(ba::io_context& io_context, short port, size_t bulk_size)
            : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), bulk_size_(bulk_size)
        {
            do_accept();
        }

    private:
        void do_accept()
        {
            acceptor_.async_accept(
                [this](boost::system::error_code ec, tcp::socket socket)
                {
                    if (!ec)
                    {
                        std::make_shared<async_session>(std::move(socket), bulk_size_)->start();
                    }
                    do_accept();
                });
        }

        tcp::acceptor acceptor_;
        size_t        bulk_size_;
    };
    
}