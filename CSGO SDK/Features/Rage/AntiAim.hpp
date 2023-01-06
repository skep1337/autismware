#pragma once
#include "../../SDK/sdk.hpp"

namespace Interfaces
{

	class __declspec(novtable) AntiAimbot : public NonCopyable {
	public:
		static Encrypted_t<AntiAimbot> Get();
		virtual void Main(bool* bSendPacket, bool* bFinalPacket, Encrypted_t<CUserCmd> cmd, bool ragebot) = 0;
	protected:
		AntiAimbot() { };
		virtual ~AntiAimbot() { };
	};
}