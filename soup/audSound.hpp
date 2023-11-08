#pragma once

namespace soup
{
	struct audSound
	{
		virtual ~audSound() = default;

		virtual void prepare();
		[[nodiscard]] virtual bool hasFinished() noexcept;
		[[nodiscard]] virtual double getAmplitude() = 0; // Called AUD_SAMPLE_RATE times per second.
	};

	struct audSoundSimple : public audSound
	{
		double t = 0.0;

		void prepare() final;
		[[nodiscard]] bool hasFinished() noexcept final;
		[[nodiscard]] double getAmplitude() final;

		[[nodiscard]] virtual double getAmplitude(double t) = 0;
		[[nodiscard]] virtual double getDurationSeconds() noexcept;
	};
}
