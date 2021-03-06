/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <testMain.h>

#include <epicsUnitTest.h>

#include <pvxs/unittest.h>
#include <pvxs/nt.h>

namespace {

using namespace pvxs;

void testNTScalar()
{
    testDiag("In %s", __func__);

    // plain, without display meta
    auto top = nt::NTScalar{TypeCode::Int32}.create();

    testTrue(top.idStartsWith("epics:nt/NTScalar:"));

    testEq(top["value"].type(), TypeCode::Int32);
    testEq(top["display.limitLow"].type(), TypeCode::Null);
    testEq(top["display.description"].type(), TypeCode::Null);
    testEq(top["control.limitLow"].type(), TypeCode::Null);

    // with display, but not control
    top = nt::NTScalar{TypeCode::Float64,true}.create();

    testEq(top["value"].type(), TypeCode::Float64);
    testEq(top["display.limitLow"].type(), TypeCode::Float64);
    testEq(top["display.description"].type(), TypeCode::String);
    testEq(top["control.limitLow"].type(), TypeCode::Null);

    // with everything
    top = nt::NTScalar{TypeCode::Float64,true,true,true}.create();

    testEq(top["value"].type(), TypeCode::Float64);
    testEq(top["display.limitLow"].type(), TypeCode::Float64);
    testEq(top["display.description"].type(), TypeCode::String);
    testEq(top["control.limitLow"].type(), TypeCode::Float64);
}

void testNTNDArray()
{
    testDiag("In %s", __func__);

    auto top = nt::NTNDArray{}.create();

    testTrue(top.idStartsWith("epics:nt/NTNDArray:"));
}

void testNTURI()
{
    testDiag("In %s", __func__);

    using namespace pvxs::members;

    auto def = nt::NTURI({
                             UInt32("arg1"),
                             String("arg2"),
                         });

    auto top = def.call(42, "hello");

    testTrue(top.idStartsWith("epics:nt/NTURI:"));
    testEq(top["query.arg1"].as<uint32_t>(), 42u);
    testEq(top["query.arg2"].as<std::string>(), "hello");
}

} // namespace

MAIN(testnt) {
    testPlan(17);
    testNTScalar();
    testNTNDArray();
    testNTURI();
    return testDone();
}
