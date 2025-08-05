#include <gtest/gtest.h>
#include <sstream>
#include <tuple>
#include <thread>

#ifndef __PRETTY_FUNCTION__
#include "pretty.h"
#endif
#include "async_internal.h"
#include "async.h"

using namespace otus_hw7;
using namespace otus_hw9;

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

using namespace std::literals::string_literals;

TEST(test_async, test_q)
{
    ICommandQueuePtr_t cmd_q = otus_hw9::create_command_queue(ICommandQueue::Type::qInput);
    EXPECT_TRUE( cmd_q );
    ICommandCreatorPtr_t cmd_creator{std::make_unique<CommandCreator>()};
    cmd_q->push(cmd_creator->create_command("Test data", 0));
    EXPECT_EQ(cmd_q->size(), 1);
    ICommandPtr_t cmd = cmd_creator->create_command("Test data 2", 0);
    cmd_q->push(std::move(cmd));
    EXPECT_EQ(cmd_q->size(), 2);
    cmd_q->pop(cmd);
    EXPECT_EQ(cmd_q->size(), 1);
}

TEST(test_async, test_create_q)
{
    ICommandQueuePtr_t cmd_q =  otus_hw9::create_command_queue(ICommandQueue::Type::qInput);
    EXPECT_TRUE( cmd_q );
}

TEST(test_async, test_iosstream)
{
    std::stringstream iostm(std::ios_base::in|std::ios_base::out|std::ios_base::ate|std::ios_base::app);
    iostm << "1, 2, 3, 4, 5, 6, 7" << std::endl;
    iostm << "10, 12, 13, 14, 15, 16, 17" << std::endl;
    
    std::string s;
    EXPECT_TRUE( std::getline(iostm, s) );
    EXPECT_EQ(s, "1, 2, 3, 4, 5, 6, 7");
    iostm << "20, 22, 23";

    EXPECT_TRUE( std::getline(iostm, s) );
    EXPECT_EQ(s, "10, 12, 13, 14, 15, 16, 17");

    EXPECT_TRUE( std::getline(iostm, s) );
    EXPECT_EQ(s, "20, 22, 23");

    EXPECT_FALSE( std::getline(iostm, s) );

    if( !iostm )
        iostm.clear();
    iostm << "30, 32, 33" << std::endl;
    EXPECT_TRUE( iostm );
    EXPECT_TRUE( std::getline(iostm, s) );
    EXPECT_EQ(s, "30, 32, 33");

    EXPECT_FALSE( std::getline(iostm, s) );
}

TEST(test_async, test_connect)
{
    libasync_ctx_t ctx0 = connect(5);
    EXPECT_TRUE(ctx0);
    disconnect(ctx0);
}

TEST(test_async, test_receive)
{
    using namespace std;

    libasync_ctx_t ctx0 = connect(3);
    EXPECT_TRUE(ctx0);
    
    auto inp_s = "1"s; 
    int rc = receive(ctx0, inp_s.c_str(), inp_s.length());
    EXPECT_EQ(rc, 0);

    inp_s = "2\n3"s; 
    rc = receive(ctx0, inp_s.c_str(), inp_s.length());
    EXPECT_EQ(rc, 0);

    inp_s = "10\n"s; 
    rc = receive(ctx0, inp_s.c_str(), inp_s.length());
    EXPECT_EQ(rc, 0);

    inp_s = "{\n"s; 
    rc = receive(ctx0, inp_s.c_str(), inp_s.length());
    EXPECT_EQ(rc, 0);

    inp_s = "20\n"s; 
    rc = receive(ctx0, inp_s.c_str(), inp_s.length());
    EXPECT_EQ(rc, 0);

    inp_s = "30\n"s; 
    rc = receive(ctx0, inp_s.c_str(), inp_s.length());
    EXPECT_EQ(rc, 0);

    inp_s = "}\n"s; 
    rc = receive(ctx0, inp_s.c_str(), inp_s.length());
    EXPECT_EQ(rc, 0);

    inp_s = "21\n22\n23\n24"s; 
    rc = receive(ctx0, inp_s.c_str(), inp_s.length());
    EXPECT_EQ(rc, 0);

    rc = disconnect(ctx0);
    EXPECT_EQ(rc, 0);
}


TEST(test_async, test_receive_mt)
{
    using namespace std;

    thread t1([](){
            libasync_ctx_t ctx0 = connect(3);
            EXPECT_TRUE(ctx0);
            
            string inp_s;
            int rc{}; 
            inp_s = "1-1\n1-2\n1-3"s; 
            rc = receive(ctx0, inp_s.c_str(), inp_s.length());
            EXPECT_EQ(rc, 0);

            inp_s = "1-10\n{\n1-20\n1-30\n1-40"s; 
            rc = receive(ctx0, inp_s.c_str(), inp_s.length());
            EXPECT_EQ(rc, 0);

            inp_s = "1-50\n}\n1-21\n1-22\n1-23\n1-24"s; 
            rc = receive(ctx0, inp_s.c_str(), inp_s.length());
            EXPECT_EQ(rc, 0);

            rc = disconnect(ctx0);
            EXPECT_EQ(rc, 0);
        }
    );

    thread t2([](){
            libasync_ctx_t ctx0 = connect(2);
            EXPECT_TRUE(ctx0);
            
            auto inp_s = "2-1\n2-2\n2-3"s; 
            int rc = receive(ctx0, inp_s.c_str(), inp_s.length());
            EXPECT_EQ(rc, 0);

            inp_s = "2-10\n{\n2-20\n2-30\n}2-40"s; 
            rc = receive(ctx0, inp_s.c_str(), inp_s.length());
            EXPECT_EQ(rc, 0);

            inp_s = "2-21\n}\n2-22\n2-23\n2-24"s; 
            rc = receive(ctx0, inp_s.c_str(), inp_s.length());
            EXPECT_EQ(rc, 0);

            rc = disconnect(ctx0);
            EXPECT_EQ(rc, 0);
        }
    );    
    
    EXPECT_TRUE( t1.joinable() );
    EXPECT_TRUE( t2.joinable() );
    
    t1.join();
    t2.join();
}
