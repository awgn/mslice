#include <mslice.hpp>

#include <yats.hpp>

using namespace yats;


Context(mslice)
{

    Test(simple)
    {
        mem::slice_allocator<int> alloc;

        auto s1 = alloc.new_slice(std::forward_as_tuple(42));

        Assert( *mem::get<0>(*s1) == 42 );

        auto s2 = alloc.new_slice(mem::none);

        Assert( *mem::get<0>(*s2) == 0 );
    }


    Test(slice_of_pair)
    {
        mem::slice_allocator<int, std::string> alloc;

        auto s1 = alloc.new_slice(std::forward_as_tuple(42), mem::none);

        Assert( *mem::get<0>(s1) == 42 );
        Assert( *mem::get<1>(s1) == std::string() );

        auto s2 = alloc.new_slice(mem::none, std::forward_as_tuple("hello world"));

        Assert( *mem::get<0>(s2) == 0 );
        Assert( *mem::get<1>(s2) == std::string("hello world") );
    }


    Test(new_shared)
    {
        mem::slice_allocator<int> alloc;

        auto s1 = alloc.new_shared<int>(1);
        auto s2 = alloc.new_shared<int>(2);
        auto s3 = alloc.new_shared<int>(3);
        auto s4 = alloc.new_shared<int>(4);

        Assert( *s1 == 1);
        Assert( *s2 == 2);
        Assert( *s3 == 3);
        Assert( *s4 == 4);
    }


    struct object
    {
        object(int v = -1)
        : value_(v)
        {}

        int value_;
    };

    Test(default_ctor)
    {
        mem::slice_allocator<object> alloc;

        auto o1 = alloc.new_shared<object>();
        auto o2 = alloc.new_shared<object>(42);

        Assert(o1->value_ == -1);
        Assert(o2->value_ == 42);
    }

    Test(memanger_renewal)
    {
        mem::basic_slice_allocator<2, int> alloc;

        auto s1 = alloc.new_shared<int>(1);
        auto s2 = alloc.new_shared<int>(2);
        auto s3 = alloc.new_shared<int>(3);
        auto s4 = alloc.new_shared<int>(4);

        Assert( *s1 == 1);
        Assert( *s2 == 2);
        Assert( *s3 == 3);
        Assert( *s4 == 4);
    }
}


int
main(int argc, char * argv[])
{
    return yats::run(argc, argv);
}


