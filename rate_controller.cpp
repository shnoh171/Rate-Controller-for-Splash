#include <iostream>
#include <fstream>
#include <thread>
#include <queue>
#include <ctime>
#include <vector>

#define stime_t std::chrono::time_point<std::chrono::steady_clock>
#define sduration_t std::chrono::duration<double, std::milli>
#define sduration_cast std::chrono::duration_cast<std::chrono::duration<double>>

struct example_data
{
    long birth_mark;
    bool extra_signal;
    int data;
};

example_data CreateDataItem(int data);

example_data CreateDataItem(int data)
{
    example_data d;

    auto epoch = (std::chrono::steady_clock::now()).time_since_epoch();
    auto val = std::chrono::duration_cast<std::chrono::milliseconds>(epoch);
    d.birth_mark = val.count();
    d.extra_signal = false;
    d.data = data;

    return d;
}

template <typename data0>
class RateController
{
    private:
        int rate; // Hz
        std::queue<data0> output_queue;
        int freshness; // ms
        int max_queue_size;
        stime_t init_time;
        stime_t prev_birth_mark;

        long TimePointToLong(stime_t tp);
        data0 CreateExtrapolationCommand(stime_t ec_birth_mark);
        stime_t LongToTimePoint(long cnt);

        bool IsOutputQueueFull();
        void GenerateOutput();
        void SendDataItem(data0 d);
        void SendExtrapolationCommand(stime_t ec_birth_mark);

    public:
        RateController(int rate_constraint, int freshness_constraint); // (Hz, ms)
        void StartRateControl(bool generate_and_start);
        void InsertDataItem(data0 d);
};

template <typename data0>
long RateController<data0>::TimePointToLong(stime_t tp) 
{
    auto epoch = tp.time_since_epoch();
    auto val = std::chrono::duration_cast<std::chrono::milliseconds>(epoch);
    long ret = val.count();

    return ret;
}

template <typename data0>
data0 RateController<data0>::CreateExtrapolationCommand(stime_t ec_birth_mark)
{
    data0 d;
    d.birth_mark = TimePointToLong(ec_birth_mark);
    d.extra_signal = true;
    
    return d;
}

template <typename data0>
stime_t RateController<data0>::LongToTimePoint(long cnt) 
{
    std::chrono::milliseconds dur(cnt);
    std::chrono::time_point<std::chrono::steady_clock> ret(dur);
    
    return ret;
}

template <typename data0>
bool RateController<data0>::IsOutputQueueFull()
{
    if (output_queue.size() >= max_queue_size)
        return true;
    else
        return false;
}

template <typename data0>
void RateController<data0>::GenerateOutput()
{
    data0 d;
    bool extrapolation_flag = true;

    while (!output_queue.empty()) {
        d = output_queue.front();
        output_queue.pop();

        stime_t d_birth_mark = LongToTimePoint(d.birth_mark);

        if (d_birth_mark >= prev_birth_mark) {
            extrapolation_flag = false;
            break;
        }
    }

    if (extrapolation_flag) {
        stime_t ec_birth_mark = prev_birth_mark + std::chrono::milliseconds(1000/rate);
        d = CreateExtrapolationCommand(ec_birth_mark);
    }
    
    SendDataItem(d);
    prev_birth_mark = LongToTimePoint(d.birth_mark);
}

template <typename data0>
void RateController<data0>::SendDataItem(data0 d)
{
    // TODO: Replace with real data send implementation on integration phase
    stime_t curr_time = std::chrono::steady_clock::now();
    sduration_t time_span_curr = sduration_cast(curr_time - init_time);
    sduration_t time_span_birth = sduration_cast(LongToTimePoint(d.birth_mark) - init_time);

    std::cout << "curr: " << time_span_curr.count();
    std::cout << ", birth: " << time_span_birth.count() << ", ";
    if (d.extra_signal) std::cout << "e\n";
    else std::cout << d.data << "\n";
}

template <typename data0>
RateController<data0>::RateController(int rate_constraint, int freshness_constraint) {
    rate = rate_constraint;
    freshness = freshness_constraint;
    max_queue_size = rate_constraint * freshness_constraint / 1000;
}

template <typename data0>
void RateController<data0>::StartRateControl(bool generate_and_start)
{
    stime_t next_time = std::chrono::steady_clock::now();
    init_time = next_time;
    if (generate_and_start) {
        while (true) {
            GenerateOutput();
            next_time = next_time + std::chrono::milliseconds(1000/rate);
            std::this_thread::sleep_until(next_time);
        }
    } else {
        while (true) {
            next_time = next_time + std::chrono::milliseconds(1000/rate);
            std::this_thread::sleep_until(next_time);
            GenerateOutput();
        }
    }
}

template <typename data0>
void RateController<data0>::InsertDataItem(data0 d)
{
    if(!IsOutputQueueFull()) {
        output_queue.push(d);
    } else {
        output_queue.pop();
        output_queue.push(d);
    }
}

int main()
{
    // Sample workload
    RateController<example_data> *rc_ptr = new RateController<example_data>(10, 400);

    std::thread th(&RateController<example_data>::StartRateControl, rc_ptr, false);

    std::chrono::milliseconds dura1(100);
    std::this_thread::sleep_for(dura1);
    rc_ptr->InsertDataItem(CreateDataItem(1)); // 100
    std::chrono::milliseconds dura2(100);
    std::this_thread::sleep_for(dura2);
    rc_ptr->InsertDataItem(CreateDataItem(2)); // 200
    std::chrono::milliseconds dura3(100);
    std::this_thread::sleep_for(dura3);
    rc_ptr->InsertDataItem(CreateDataItem(3)); // 300
    std::chrono::milliseconds dura4(1400);
    std::this_thread::sleep_for(dura4);
    rc_ptr->InsertDataItem(CreateDataItem(4)); // 1700
    std::chrono::milliseconds dura5(600);
    std::this_thread::sleep_for(dura5);
    rc_ptr->InsertDataItem(CreateDataItem(5)); // 2300

    th.join();

    return 0;
}