#pragma once

#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <utility>

namespace thread_pool {
namespace data_structures {
namespace thread_safe {
namespace lock_based {

template <typename T> class queue {
	static_assert(std::is_nothrow_move_constructible<T>::value,
				  "The thread-safe queue requires nothrowable move-constructible elements");

  public:
	queue() : _head{std::make_unique<node>()}, _tail{_head.get()} {
	}

	queue(const queue&) = delete;

	queue& operator=(const queue&) = delete;

	queue(queue&&) = default;

	queue& operator=(queue&&) = default;

	~queue() = default;

	void push(T&& data) {
		std::unique_ptr<node> tail = std::make_unique<node>(std::move(data));

		{
			std::lock_guard<std::mutex> lock{_tail_guard};

			node* const new_tail = tail.get();
			_tail->next_node = std::move(tail);
			_tail = new_tail;
		}

		_notifier.notify_one();
	}

	T pop() {
		std::lock_guard<std::mutex> lock{_head_guard};

		if (_head.get() != this->get_tail()) {
			T head = std::move(_head->next_node->data);
			_head = std::move(_head->next_node);
			return head;
		}
		else
			throw std::logic_error{"All data was already popped!"};
	}

	T wait_pop() {
		std::unique_lock<std::mutex> lock{_head_guard};

		_notifier.wait(lock, [this]() { return _head.get() != this->get_tail(); });

		T head = std::move(_head->next_node->data);
		_head = std::move(_head->next_node);
		return head;
	}

	bool empty() const {
		std::lock_guard<std::mutex> lock{_head_guard};

		return _head.get() == this->get_tail();
	}

  private:
	struct node {
		node() = default;

		explicit node(T&& data) : node{std::move(data), nullptr} {
		}

		explicit node(T&& data, node* next_node) : data{std::move(data)}, next_node{next_node} {
		}

		node(const node&) = delete;

		node& operator=(const node&) = delete;

		node(node&&) = default;

		node& operator=(node&&) = default;

		~node() = default;

		T data;
		std::unique_ptr<node> next_node;
	};

	node* get_tail() const {
		std::lock_guard<std::mutex> lock{_tail_guard};

		return _tail;
	}

	mutable std::mutex _head_guard;
	std::unique_ptr<node> _head;
	// TODO Research if it makes sense to protect the tail with a read/write lock
	mutable std::mutex _tail_guard;
	node* _tail;
	std::condition_variable _notifier;
};

/**
 * @brief A thread-safe wrapper of std::queue
 * @tparam T Any move-constructable type which does not in any way lock the guard of this queue in its copy/move
 * constructor
 */

template <typename T> class std_queue {
	static_assert(std::is_nothrow_move_constructible<T>::value,
				  "The thread-safe queue requires nothrowable move-constructible elements");

  public:
	void push(T&& element) {
		std::lock_guard<std::mutex> lock{_guard};

		_queue.emplace(std::move(element));

		_notifier.notify_one();
	}

	T pop() {
		std::lock_guard<std::mutex> lock{_guard};

		if (_queue.empty())
			throw std::logic_error{"All data was already popped!"};

		T front = std::move(_queue.front());
		_queue.pop();
		return front;
	}

	T wait_pop() {
		std::unique_lock<std::mutex> lock{_guard};

		_notifier.wait(lock, [this]() { return !_queue.empty(); });

		T front = std::move(_queue.front());
		// Note thst pop won't throw because the notifier waits until the queue is not empty.
		_queue.pop();
		return front;
	}

  private:
	std::mutex _guard;
	std::condition_variable _notifier;
	std::queue<T> _queue;
};

} // namespace lock_based
} // namespace thread_safe
} // namespace data_structures
} // namespace thread_pool