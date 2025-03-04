#ifndef OIP_BLOCKING_QUEUE_H
#define OIP_BLOCKING_QUEUE_H

#include <godot_cpp/variant/string.hpp>

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace godot {

class OIPBlockingQueue {

private:
	std::queue<String> queue;
	std::mutex mutex;
	std::condition_variable cv;
	bool stop = false;

public:
	void push(const String message);
    String pop();
    void shutdown();
};

} //namespace godot

#endif
