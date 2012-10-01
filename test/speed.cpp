#include <cstdio>
#include <string>
#include <stdexcept>
#include <thread>
#include <array>
#include <vector>
#include <memory>
#include <chrono>

#include <iostream>

#include <mslice.hpp>

namespace vt100
{
    const char * const BOLD  = "\E[1m";
    const char * const RESET = "\E[0m";
}


struct sparse_counter
{
    volatile long long value;

} __attribute__((aligned(64)));


sparse_counter counters[64];


typedef std::array<char, 64> target_type;


// allocator policies
//


struct raw_allocator
{
    std::unique_ptr<target_type>
    operator()() 
    {
        return std::move(std::unique_ptr<target_type>(new target_type));
    }
};


struct shared_allocator
{
    std::shared_ptr<target_type>
    operator()() 
    {
        return std::move(std::make_shared<target_type>());
    }
};


struct mslice_allocator
{
    mslice_allocator()
    : alloc()
    {}
    
    std::shared_ptr<mem::slice<target_type>>
    operator()() 
    {
        auto sp = alloc.new_slice();
        return std::move(sp);
    }

    mem::slice_allocator<target_type> alloc;
};


// worker thread: 
//

template <typename Tp, typename Alloc>
struct worker
{
    void operator()(int id, size_t len)
    {
        std::vector<Tp> buffer;

        buffer.reserve(len);

        unsigned long long int n = 0, last = 0;

        std::chrono::time_point<std::chrono::system_clock> last_tp;

        auto S = (1<<22) - 1;

        Alloc allocator;

        for(;;)
        {
            if ((n % len) == 0) 
            {
                buffer.clear();
            }

            auto p = allocator();

            buffer.push_back(std::move(p));

            n++;

            if ( !(n & S) )
            {
                auto now = std::chrono::system_clock::now();
                auto diff = now - last_tp;

                counters[id].value = (n - last) * 1000000 / std::chrono::duration_cast<std::chrono::microseconds>(diff).count(); 

                last_tp = now;
                last    = n;
            }
        }
    }
};


int
main(int argc, char *argv[])
{
    std::vector<std::thread> ws;

    if (argc < 4)
        throw std::runtime_error(std::string("usage: ").append(argv[0]).append(" mode #thread buff_len"));

    const unsigned int mode    = atoi(argv[1]);
    const unsigned int nthread = atoi(argv[2]);
    const unsigned int buflen  = atoi(argv[3]);

    for(unsigned int i = 0; i < nthread; i++)
    {
        switch(mode)
        {
            case 0:
            {
                std::thread t(worker<std::unique_ptr<target_type>, raw_allocator>(), i, buflen); 
                ws.push_back(std::move(t));
            } break;

            case 1:
            {
                std::thread t(worker<std::shared_ptr<target_type>, shared_allocator>(), i, buflen); 
                ws.push_back(std::move(t));
            } break;

            case 2:
            {
                std::thread t(worker<std::shared_ptr<mem::slice<target_type>>, mslice_allocator>(), i, buflen); 
                ws.push_back(std::move(t));
            } break;

            default:
                throw std::runtime_error("mode not implemented");
        }
    }

    std::vector<const char *> mode_name = { "malloc", "malloc+shared_ptr", "slice_allocator" };

    for(;;) 
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        unsigned long long t = 0;
        for(auto x : counters)
        {
            t += x.value;
        }

        std::cout << mode_name[mode] << ": " 
                  << vt100::BOLD << static_cast<double>(t)/1000000 
                  << vt100::RESET << " Malloc/sec (million allocations per second)" << std::endl;
    }

    ws[0].join();

    return 0;
}

