#pragma once

namespace Components
{
	class ArenaLength : public Component
	{
	public:
		ArenaLength();

#if defined(DEBUG) || defined(FORCE_UNIT_TESTS)
		const char* getName() override { return "ArenaLength"; };
#endif

		static Game::newMapArena_t NewArenas[128];

	private:
		static void ArenaMapOffsetHook1();
		static void ArenaMapOffsetHook2();
		static void ArenaMapOffsetHook3();
		static void ArenaMapOffsetHook4();
	};
}
