#include "oip_blocking_queue.h"

using namespace godot;

void OIPBlockingQueue::push(const String message) {
	{
		std::lock_guard<std::mutex> lock(mutex);
		queue.push(message);
	}
	cv.notify_one(); // Wake up one waiting thread
}

String OIPBlockingQueue::pop() {
	std::unique_lock<std::mutex> lock(mutex);
	cv.wait(lock, [this]() { return !queue.empty() || stop; });

	if (stop && queue.empty()) {
		return "";
	}

	String message = queue.front();
	queue.pop();
	return message;
}

void OIPBlockingQueue::shutdown() {
	{
		std::lock_guard<std::mutex> lock(mutex);
		stop = true;
	}
	cv.notify_all(); // Wake up all threads so they can exit
}
