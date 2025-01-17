#pragma once

#include "Task.hpp"

#include "Fiber.hpp"

NAMESPACE_SOUP
{
	template <typename Base = Task>
	class FiberTask : public Base
	{
	private:
		Fiber fiber;

	public:
		FiberTask()
			: fiber([](Capture&& cap)
			{
				cap.get<FiberTask*>()->run();
				cap.get<FiberTask*>()->setWorkDone();
			}, this)
		{
		}

	protected:
		void onTick() final
		{
			fiber.run();
		}

		virtual void run() = 0;

		void yield()
		{
			// Fiber::curent()->yield();
			fiber.yield();
		}
	};
}
