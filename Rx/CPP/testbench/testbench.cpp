// Copyright (c) Microsoft Open Technologies, Inc. All rights reserved. See License.txt in the project root for license information.

// testbench.cpp : Defines the entry point for the console application.
//

#include "cpprx/rx.hpp"
#include "cpplinq/linq.hpp"

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>
#include <exception>
#include <regex>

using namespace std;

bool IsPrime(int x);

void PrintPrimes(int n)
{
    std::cout << "first " << n << " primes squared" << endl;
    auto values = rxcpp::Range(2); // infinite (until overflow) stream of integers
    auto s1 = rxcpp::from(values)
        .where(IsPrime)
        .select([](int x) { return std::make_pair(x,  x*x); })
        .take(n)
        .subscribe(
            [](pair<int, int> p) {
                cout << p.first << " =square=> " << p.second << endl;
            });
}

void Combine(int n)
{
#if RXCPP_USE_VARIADIC_TEMPLATES
    auto input1 = std::make_shared<rxcpp::EventLoopScheduler>();
    auto input2 = std::make_shared<rxcpp::EventLoopScheduler>();
    auto output = std::make_shared<rxcpp::EventLoopScheduler>();

    auto values1 = rxcpp::Range(100); // infinite (until overflow) stream of integers
    auto s1 = rxcpp::from(values1)
        .subscribe_on(input1)
        .where(IsPrime)
        .select([](int prime){this_thread::yield(); return prime;})
        .publish();

    auto values2 = rxcpp::Range(2); // infinite (until overflow) stream of integers
    rxcpp::from(values2)
        .subscribe_on(input2)
        .where(IsPrime)
        .select([](int prime){this_thread::yield(); return prime;})
        .combine_latest(s1)
        .take(n)
        .observe_on(output)
        .for_each(
            [](tuple<int, int> p) {
                cout << get<0>(p) << " =combined=> " << get<1>(p) << endl;
            });
#else
    (void)n;
#endif //RXCPP_USE_VARIADIC_TEMPLATES
}

struct Count {
    Count() : nexts(0), completions(0), errors(0), disposals(0) {}
    std::atomic<int> nexts;
    std::atomic<int> completions;
    std::atomic<int> errors;
    std::atomic<int> disposals;
};
template <class T>
std::shared_ptr<rxcpp::Observable<T>> Record(
    const std::shared_ptr<rxcpp::Observable<T>>& source,
    Count* count
    )
{
    return rxcpp::CreateObservable<T>(
        [=](std::shared_ptr<rxcpp::Observer<T>> observer)
        {
            rxcpp::ComposableDisposable cd;
            cd.Add(rxcpp::Disposable([=](){
                ++count->disposals;}));
            cd.Add(rxcpp::Subscribe(
                source,
            // on next
                [=](T element)
                {
                    ++count->nexts;
                    observer->OnNext(std::move(element));
                },
            // on completed
                [=]
                {
                    ++count->completions;
                    observer->OnCompleted();
                },
            // on error
                [=](const std::exception_ptr& error)
                {
                    ++count->errors;
                    observer->OnError(error);
                }));
            return cd;
        });
}
struct record {};
template<class T>
rxcpp::Binder<std::shared_ptr<rxcpp::Observable<T>>> rxcpp_chain(record&&, const std::shared_ptr<rxcpp::Observable<T>>& source, Count* count) {
    return rxcpp::from(Record(source, count));
}

template<class InputScheduler, class OutputScheduler>
void Zip(int n)
{
#if RXCPP_USE_VARIADIC_TEMPLATES
    auto input1 = std::make_shared<InputScheduler>();
    auto input2 = std::make_shared<InputScheduler>();
    auto output = std::make_shared<OutputScheduler>();

    Count s1count, s2count, zipcount, takecount, outputcount;

    auto values1 = rxcpp::Range(100); // infinite (until overflow) stream of integers
    auto s1 = rxcpp::from(values1)
        .subscribe_on(input1)
        .where(IsPrime)
        .template chain<record>(&s1count)
        .select([](int prime){this_thread::yield(); return prime;})
        .publish();

    auto values2 = rxcpp::Range(2); // infinite (until overflow) stream of integers
    rxcpp::from(values2)
        .subscribe_on(input2)
        .where(IsPrime)
        .template chain<record>(&s2count)
        .select([](int prime){this_thread::yield(); return prime;})
        .zip(s1)
        .template chain<record>(&zipcount)
        .take(n)
        .template chain<record>(&takecount)
        .observe_on(output)
        .template chain<record>(&outputcount)
        .for_each(
            [](tuple<int, int> p) {
                cout << get<0>(p) << " =zipped=> " << get<1>(p) << endl;
            });

    cout << "location: nexts, completions, errors, disposals" << endl;
    cout << "s1count:" << s1count.nexts << ", " << s1count.completions << ", " << s1count.errors << ", " << s1count.disposals << endl;
    cout << "s2count:" << s2count.nexts << ", " << s2count.completions << ", " << s2count.errors << ", " << s2count.disposals << endl;
    cout << "zipcount:" << zipcount.nexts << ", " << zipcount.completions << ", " << zipcount.errors << ", " << zipcount.disposals << endl;
    cout << "takecount:" << takecount.nexts << ", " << takecount.completions << ", " << takecount.errors << ", " << takecount.disposals << endl;
    cout << "outputcount:" << outputcount.nexts << ", " << outputcount.completions << ", " << outputcount.errors << ", " << outputcount.disposals << endl;
#else
    (void)n;
#endif //RXCPP_USE_VARIADIC_TEMPLATES
}

void Merge(int n)
{
#if RXCPP_USE_VARIADIC_TEMPLATES
    auto input1 = std::make_shared<rxcpp::EventLoopScheduler>();
    auto input2 = std::make_shared<rxcpp::EventLoopScheduler>();
    auto output = std::make_shared<rxcpp::EventLoopScheduler>();

    cout << "merge==> <source>: <prime>" << endl;

    auto values1 = rxcpp::Range(100); // infinite (until overflow) stream of integers
    auto s1 = rxcpp::from(values1)
        .subscribe_on(input1)
        .where(IsPrime)
        .select([](int prime1) {this_thread::yield(); return std::make_tuple("1: ", prime1);})
        .publish();

    auto values2 = rxcpp::Range(2); // infinite (until overflow) stream of integers
    rxcpp::from(values2)
        .subscribe_on(input2)
        .where(IsPrime)
        .select([](int prime2) {this_thread::yield(); return std::make_tuple("2: ", prime2);})
        .merge(s1)
        .take(n)
        .observe_on(output)
        .for_each(
            [](tuple<const char*, int> p) {
                cout << get<0>(p) << get<1>(p) << endl;
            });
#else
    (void)n;
#endif //RXCPP_USE_VARIADIC_TEMPLATES
}

std::shared_ptr<rxcpp::Observable<string>> Data(
    string filename,
    rxcpp::Scheduler::shared scheduler = std::make_shared<rxcpp::CurrentThreadScheduler>()
);
string extract_value(const string& input, const string& key);

void run()
{
    using namespace cpplinq;

    struct item {
        string args;
        int    concurrency;
        double time;

        item(const string& input) {
            args =              extract_value(input, "args");
            concurrency = atoi( extract_value(input, "concurrency").c_str() );
            time =        atof( extract_value(input, "time").c_str() );
        }
    };

    auto input = std::make_shared<rxcpp::EventLoopScheduler>();
    auto output = std::make_shared<rxcpp::EventLoopScheduler>();

    auto dataLines = Data("data.txt");

    int arggroupcount = 0;

    rxcpp::from(dataLines)
        .subscribe_on(input)
        // parse input into items
        .select([](const string& line) { 
            return item(line);}
        )
        // group items by args field
        .group_by([](const item& i) {
            return i.args;}
        )
        .select([=](const std::shared_ptr<rxcpp::GroupedObservable<std::string, item>> & gob){
            return std::make_pair(
                gob->Key(), // keep args key
                rxcpp::from(gob)
                    // group items by concurrency field
                    .group_by([](const item& i){
                        return i.concurrency;}
                    )
                    // select only the times field
                    .select([=] (const std::shared_ptr<rxcpp::GroupedObservable<int, item>> & gob){
                        return std::make_pair(
                            gob->Key(), // keep concurrency key
                            rxcpp::from(gob)
                                .select([](const item& i){
                                    return i.time;}
                                )
                                .publish());}
                    )
                    .publish());}
        )
        .observe_on(output)
        // print the grouped results
        // for_each the args to block until the nested subscribes have finished
        .for_each([&](const std::pair<std::basic_string<char>, std::shared_ptr<rxcpp::Observable<std::pair<int, std::shared_ptr<rxcpp::Observable<double> > > > > >& ob){
            auto argsstate = std::make_shared<std::tuple<bool, std::string, int>>(false, ob.first, arggroupcount++);
            rxcpp::from(ob.second)
                // subscribe the concurrencies within each args
                .subscribe([=](const std::pair<int, std::shared_ptr<rxcpp::Observable<double> > >& ob){
                    auto linestate = std::make_shared<std::pair<int, std::vector<double>>>(ob.first, std::vector<double>());
                    rxcpp::from(ob.second)
                        // subscribe the times within each concurrency
                        .subscribe([=](const double& i){
                            // collect the times
                            linestate->second.push_back(i);
                        },[=]{
                            // this concurrency's times are complete
                            // output a line to the console.

                            if (!std::get<0>(*argsstate))
                            {
                                std::get<0>(*argsstate) = true;
                                cout<<"arguments: "<<std::get<1>(*argsstate)<<endl;
                                cout << "concurrency, mean, |, raw_data," << endl;
                            }

                            cout << linestate->first << ", ";
                            
                            auto n = from(linestate->second).count();
                            auto sum = std::accumulate(linestate->second.begin(), linestate->second.end(), 0.0);

                            cout << (sum / n) << ", |";

                            for (auto timeIter = linestate->second.begin(), end = linestate->second.end();
                                timeIter != end;
                                ++timeIter)
                            {
                                cout << ", " << *timeIter;
                            }
                            cout << endl;
                        });
                });
        });
}

template<class Scheduler>
void innerScheduler() {
    auto outer = std::make_shared<Scheduler>();
    rxcpp::Scheduler::shared inner;
    outer->Schedule([&](rxcpp::Scheduler::shared s){
        inner = s; return rxcpp::Disposable::Empty();});
    while(!inner);
    inner->Schedule([&](rxcpp::Scheduler::shared s){
        inner = nullptr; return rxcpp::Disposable::Empty();});
    while(!!inner);
    cout << "innerScheduler test succeeded" << endl;
}

int main(int argc, char* argv[])
{
    try {
        PrintPrimes(20);
        cout << "Zip Immediate" << endl;
        Zip<rxcpp::ImmediateScheduler, rxcpp::ImmediateScheduler>(20);
        cout << "Zip Current" << endl;
        Zip<rxcpp::CurrentThreadScheduler, rxcpp::CurrentThreadScheduler>(20);
        cout << "Zip EventLoop" << endl;
        Zip<rxcpp::EventLoopScheduler, rxcpp::EventLoopScheduler>(20);
        Combine(20);
        Merge(20);

        innerScheduler<rxcpp::ImmediateScheduler>();
        innerScheduler<rxcpp::CurrentThreadScheduler>();
        innerScheduler<rxcpp::EventLoopScheduler>();

        run();
        
    } catch (exception& e) {
        cerr << "exception: " << e.what() << endl;
    }
}

bool IsPrime(int x)
{
    if (x < 2) return false;
    for (int i = 2; i <= x/2; ++i)
    {
        if (x % i == 0) 
            return false;
    }
    return true;
}

regex key_value_pair("'([^\']*)'\\s*[:,]\\s*(\\d+(?:\\.\\d+)?|'[^']*')");

string extract_value(const string& input, const string& key)
{
    const std::sregex_iterator end;
    for (std::sregex_iterator i(input.cbegin(), input.cend(), key_value_pair);
        i != end;
        ++i)
    {
        if ((*i)[1] == key)
        {
            return (*i)[2];
        }
    }
    throw std::range_error("search key not found");
}

std::shared_ptr<rxcpp::Observable<string>> Data(
    string filename,
    rxcpp::Scheduler::shared scheduler
)
{
    return rxcpp::CreateObservable<string>(
        [=](std::shared_ptr<rxcpp::Observer<string>> observer) 
        -> rxcpp::Disposable
        {
            struct State 
            {
                State(string filename) 
                    : cancel(false), data(filename) {
                        if (data.fail()) {
                            throw logic_error("could not find file");
                        }
                    }
                bool cancel;
                ifstream data;
            };
            auto state = std::make_shared<State>(std::move(filename));

            rxcpp::ComposableDisposable cd;

            cd.Add(rxcpp::Disposable([=]{
                state->cancel = true;
            }));

            cd.Add(scheduler->Schedule(
                rxcpp::fix0([=](rxcpp::Scheduler::shared s, std::function<rxcpp::Disposable(rxcpp::Scheduler::shared)> self) -> rxcpp::Disposable
                {
                    try {
                        if (state->cancel)
                            return rxcpp::Disposable::Empty();

                        string line;
                        if (!!getline(state->data, line))
                        {
                            observer->OnNext(std::move(line));
                            return s->Schedule(std::move(self));
                        }
                        else
                        {
                            observer->OnCompleted();
                        }
                    } catch (...) {
                        observer->OnError(std::current_exception());
                    }     
                    return rxcpp::Disposable::Empty();           
                })
            ));

            return cd;
        }
    );
}
