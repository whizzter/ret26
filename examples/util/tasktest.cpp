#include "ret26/task.hpp"

#include <iostream>


int main() {
	using namespace ret26;

	std::cout << "1: Pre-taskwork!\n";

    // make a pending task, this will halt things in the async code
	auto [pending_task, pending_completer] = ret26::task<int>::pending();

    // NOTICE: prefer using copying values for 
    // async sub-task that will wait on the pending.
	auto sub = [=](int y) -> ret26::task<int> {
		std::cout << "3: Sub enter\n";
        if (pending_task) {
            std::cout<<"Pseudo-4: Task already complete in previous run\n";
        }
		int rv = y * 2 + co_await pending_task;
		std::cout << "5: Sub-leave\n";
		co_return rv;
		};
	auto pretask = [=](int x) -> ret26::task<double> {
		std::cout << "2: First enter:" << x << std::endl;
		std::cout << co_await sub(x + 3) << std::endl;
		std::cout << co_await sub(x + 4) << std::endl;
		std::cout << "6: Second leave" << std::endl;
		co_return 1234.3;
		};

    // start running the tasks
	auto t = pretask(123);
    // But they'll be stopped when encountering the pending task

	std::cout << "4: Pre-completer, t done?:"<<t<<"\n";
    // Complete the pending task, this will trigger execution of the rest of the async flow.
	pending_completer(1000);

	std::cout << "7: Post-completer, t done?:" << t << ":" << *t << "\n";

	return 0;
}
