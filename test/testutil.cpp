/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <epicsUnitTest.h>
#include <testMain.h>

#include <pvxs/unittest.h>
#include <pvxs/util.h>

namespace {
using namespace pvxs;

void testFill(size_t limit)
{
    testShow()<<__func__<<" "<<limit;

    MPSCFIFO<int> Q(limit);

    for(int i=0; i<4; i++)
        Q.push(std::move(i));

    testEq(Q.pop(), 0);
    testEq(Q.pop(), 1);
    testEq(Q.pop(), 2);
    testEq(Q.pop(), 3);
}

} // namespace

MAIN(testutil)
{
    testPlan(4);
    testFill(0);
    testFill(4);
    return testDone();
}
