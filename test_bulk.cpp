#include <gtest/gtest.h>
#include <sstream>
#include <list>
#include <tuple>
#ifndef __PRETTY_FUNCTION__
#include "pretty.h"
#endif
#include "bulk_internal.h"

using namespace otus_hw7;

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

using namespace std::literals::string_literals;

TEST(test_bulk, test_q)
{
    ICommandQueuePtr_t cmd_q = create_command_queue(ICommandQueue::Type::qInput);
    EXPECT_TRUE( cmd_q );
    ICommandCreatorPtr_t creator = std::make_unique<CommandCreator>();
    cmd_q->push(creator->create_command("Test data", 0));
    EXPECT_EQ(cmd_q->size(), 1);
    ICommandPtr_t cmd = creator->create_command("Test data 2", 0);
    cmd_q->push(std::move(cmd));
    EXPECT_EQ(cmd_q->size(), 2);
    cmd_q->pop(cmd);
    EXPECT_EQ(cmd_q->size(), 1);
}

TEST(test_bulk, test_create_q)
{
    ICommandQueuePtr_t cmd_q = create_command_queue(ICommandQueue::Type::qInput);
    EXPECT_TRUE( cmd_q );
}
