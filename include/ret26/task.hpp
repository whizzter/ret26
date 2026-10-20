#include <memory>
#include <coroutine>
#include <utility>
#include <type_traits>
#include <variant>
#include <functional>
#include <vector>

namespace ret26 {
	// Task can be created/defined in a couple of different ways/scenarios.
	// - Task is a coroutine that might or might not be completed when awaited.
	// - Is pre-created with a value from start
	// - Is created as a pending task with a signaller, once signalled any listeners will continue.
	// - TODO: ^- compose any / all abstractions on top of the above 

	// The main usage wrapper, acts as a reference and currently always contains it's storage on-heap for simplicity
	template<typename T>
	class task;

	// various impl classes, not for user code
	namespace taskimpl {
		// the type that controls how the compiler generates task behaviours
		template<typename T>
		struct task_promise;

		// behaviours task_promise parent to handle return_value specialization (specifically void-return issues)
		template<typename T>
		struct task_promise_return_value;


		template<typename T>
		struct task_awaitable;
	}

	template<typename T>
	class task {
		template<typename FT>
		friend struct taskimpl::task_promise;
		template<typename FT>
		friend struct taskimpl::task_promise_return_value;
		template<typename FT>
		friend struct taskimpl::task_awaitable;

		struct void_value { int sz;  }; // dummy placeholder for non-existant provided returns
		// compute the return_holder_t value to never be void (and replace with placeholder)
		using return_holder_t = typename std::conditional_t<std::is_same_v<T, void>, void_value, T>;

		using coro_t = std::coroutine_handle<ret26::taskimpl::task_promise<T>>;

		// actual state-data for a task
		struct state_t {
			// the task-state depends on the variant state
			// 0     : "uninitialized" (pending tasks can be in this state)
			// 1     : "completed" state
			// 2     : future, exception status?
			// coro_t: previously a pending flag/callback, perhaps remove?
			std::variant<
				std::monostate,
				return_holder_t,
				// TODO: exceptions? std::unique_ptr<std::exception>,
				coro_t> v;
			// coroutines (change to functions?) of dependendants on the completion of this task.
			std::vector<std::coroutine_handle<>> pending;
		};
		using shstate_t =	std::shared_ptr<state_t>;
		shstate_t state; // make one that's initially in a monostate?

		task(shstate_t in_state) : state{in_state} {}
		task(coro_t&& inc); // helper connected to async method's
	public:
		// construct pre-completed task.
		task(return_holder_t&& value) 
			: state{ std::make_shared<state_t>(std::forward(value)) }
		{}

		// do we hold a value?
		operator bool() const {
			return state->v.index() == 1;
		}
		// get value
		const return_holder_t& value() const {
			return std::get<return_holder_t>(state->v);
		}
		// get value
		const return_holder_t& operator*() const {
			return value();
		}

		// construct a pending task of type, returns the awaitable task as well as the completion function.
		static std::pair<
			task<T>,
			std::function<void(return_holder_t&&)>
		> pending() {
			auto state = std::make_shared<state_t>(); // initialize as unused
			return { {state},[state = state](return_holder_t&& v) {
				state->v = v;
				for (auto pending : state->pending)
					pending(); // activate sleeping once (or dispatch to dispatcher?)
				state->pending.resize(0); // no need for them anymore!
				} };
		}

		// special pre-compelted void-task
		static task<void> completed_void() {
			return { void_value{} };
		}

		using promise_type = taskimpl::task_promise<T>;
	};

	namespace taskimpl {

		template<typename T>
		struct task_promise_return_value {
			task<T>::shstate_t state;
			void return_value(T&& value) {
				state->v = value;
				for (auto pending : state->pending)
					pending(); // activate sleeping once (or dispatch to dispatcher?)
				state->pending.resize(0); // no need for them anymore!
				//state->emplace(std::forward(value));
				//(*state) = (typename task<T>::return_holder_t)value;
				//state->
				//state->emplace(value);
	//			state->emplace("Hej");
				state = nullptr; // don't refer to the result once completed!
				return;
			}
		};

		template<>
		struct task_promise_return_value<void> {
			task<void>::shstate_t state;
			void return_void() {
				task<void>::return_holder_t v{};
				state->v = v; // mark as finished!
				for (auto pending : state->pending)
					pending(); // activate sleeping once (or dispatch to dispatcher?)
				state->pending.resize(0); // no need for them anymore!
				state = nullptr; // don't refer to the result once completed!
				return;
			}
		};

		template<typename T>
		struct task_awaitable {
			task<T>::shstate_t state;

			bool await_ready() const noexcept {
				return state->v.index() == 1; // callee.is_done();
			}
			template<typename Recv>
			void await_suspend(std::coroutine_handle<Recv> h) {
				state->pending.push_back(h);
			}

			T await_resume() {
				if constexpr (std::is_same_v<T, void>) {
					return;
				} else {
					return std::get<typename task<T>::return_holder_t>(state->v);
				}
			}

		};

		template<typename T>
		struct task_promise : public task_promise_return_value<T> {

			std::suspend_never initial_suspend() {
				return {};
			}
			std::suspend_never final_suspend() noexcept {
				//task = nullptr;
				return {};
			}

			template<typename CA>
			task_awaitable<CA> await_transform(task<CA> t) {
				return { t.state };
			}

			ret26::task<T> get_return_object() {
				return { std::coroutine_handle<ret26::taskimpl::task_promise<T>>::from_promise(*this) };
			}

			void unhandled_exception() {
				throw;
			}

			// void* operator new(std::size_t n) noexcept {
			// 	if (void* mem = std::malloc(n)) {
			// 		return mem;
			// 	}
			// 	return nullptr;
			// }

			// void operator delete(void* p) noexcept {
			// 	std::free(p);
			// }
		};
	}

	template<typename T>
	task<T>::task(coro_t&& inc)
		: state{ std::make_shared<state_t>(std::move(inc)) }
	{
		std::get<coro_t>(state->v).promise().state = this->state;
	}

};
