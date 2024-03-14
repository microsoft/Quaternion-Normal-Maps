// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/fmath.h>

using namespace OIIO;

using namespace std;

static int nthreads = 0;  // default: use #cores threads if available
static std::vector<std::string> filenames;
static bool inverse;
static bool deriveZ = false;
static float bias = 0.0f;
static float applyBias;
static float removeBias;

static int
parse_files(int argc, const char* argv[])
{
	for (int i = 0; i < argc; i++)
		filenames.emplace_back(argv[i]);
	return 0;
}

static void
getargs(int argc, char* argv[])
{
	bool help = false;

	ArgParse ap;
	// clang-format off
	ap.options("convertNormalToQLog -- Convert from a Basis Vector Normal to a Quaternion Logarithm Normal\n"
		"Usage: convertNormalToQLog [options] inputfile outputfile\n",
		"%*", parse_files, "",
		"--help", &help, "Print help message",
		"-i", &inverse, "Convert from a Quaternion Logarithm Normal to a Basis Vector Normal",
		"-deriveZ", &deriveZ, "Calculate the Z channel of the basis normal from the XY values (Only applies to convertion from Basis Vector Normal to Quaternion Logarithm Normal)",
		"-bias %f", &bias, "Set bias for bit precision on angle from normal, positive values bias precision towards the normal, negative values bias away (default = 0, which is linear precision). For positive bias values, the formula to remove the bias to unpack the texture from 0 to 1 so it covers -1 to 1, then (Pi/4) * Abs(value)^(bias+1) * Sign(value). At a bias of 0, the default, this can be simplified to unpacking 0 to 1 so it goes from -Pi/4 to Pi/4.",
		nullptr);
	// clang-format on
	if (ap.parse(argc, (const char**)argv) < 0) {
		std::cerr << ap.geterror() << std::endl;
		ap.usage();
		exit(EXIT_FAILURE);
	}
	if (help) {
		ap.usage();
		exit(EXIT_FAILURE);
	}

	if (deriveZ && inverse)
	{
		std::cerr << "deriveZ has no effect when converting from Quaternion Logarithm Maps to Basis Vector Maps" << std::endl;
	}

	if (filenames.size() != 2) {
		std::cerr
			<< "convertNormalToQLog: Must have exactly one input and one output filename specified.\n";
		ap.usage();
		exit(EXIT_FAILURE);
	}
}

static float getZFromXY(float x, float y)
{
	float z = 1.0f - (x*x + y*y);

	if (z < std::numeric_limits<float>::epsilon())
	{
		z = 0.0f;
	}
	else 
	{
		z = sqrt(z);
	}

	return z;
}

static void calculateApplyAndRemoveBias()
{
	if (bias >= 0.0f)
	{
		removeBias = 1.0f + bias;
	}
	else {
		removeBias = (1.0f / (-bias + 1.0f));
	}

	applyBias = 1.0f / removeBias;
}

static float applyBiasThenPack(float value)
{
	float result = value / float(M_PI_4);

	result = pow(abs(result), applyBias);

	if (value < 0.0f)
	{
		result = -result;
	}

	result += 1.0f;
	result *= 0.5f;

	return result;
}

static float unpackThenRemoveBias(float value)
{
	float result = value * 2.0f - 1.0f;

	bool negative = result < 0.0f;

	result = pow(abs(result), removeBias);

	if (negative)
	{
		result = -result;
	}

	result *= float(M_PI_4);

	return result;
}

static void
convert_buffer(ImageBuf& inputImage)
{
	int channels = inputImage.nchannels();

	if (inverse)
	{
		// Convert quat Ln normal map to basis normal map
		for (ImageBuf::Iterator<float> it(inputImage); !it.done(); ++it)
		{
			float u = unpackThenRemoveBias(it[0]);
			float v = unpackThenRemoveBias(it[1]);

			// This is the half angle squared, but will be the angle later
			float angle = u * u + v * v;
			float denominator = angle;
			if (denominator < std::numeric_limits<float>::epsilon())
			{
				denominator = 1.0f;
			}

			denominator = sqrt(denominator);

			// Now it's the angle
			angle = 2.0f * sqrt(angle);

			float sinAngle = sin(angle);

			float x = (u * sinAngle) / denominator;
			float y = (v * sinAngle) / denominator;
			float z = cos(angle);

			it[0] = float((x + 1.0f) * 0.5f);
			it[1] = float((y + 1.0f) * 0.5f);
			it[2] = float((z + 1.0f) * 0.5f);
		}
	}
	else {
		// Convert basis normal map to quat Ln normal map
		for (ImageBuf::Iterator<float> it(inputImage); !it.done(); ++it)
		{
			// Unpack basis normal map
			float x = (it[0] * 2.0f) - 1.0f;
			float y = (it[1] * 2.0f) - 1.0f;
			float z = (it[2] * 2.0f) - 1.0f;

			if (deriveZ)
			{
				z = getZFromXY(x, y);
			}

			float denominator = (x*x + y * y);
			if (denominator < std::numeric_limits<float>::epsilon())
			{
				denominator = 1.0;
			}
			else {
				denominator = sqrt(denominator);
			}

			float arcCosSqrtZ = acos(sqrt(1.0f + z) / sqrt(2.0f));

			float u = (x * arcCosSqrtZ) / denominator;
			float v = (y * arcCosSqrtZ) / denominator;

			// Apply bias and scale to normal, then pack to 0-1
			it[0] = float(applyBiasThenPack(u));
			it[1] = float(applyBiasThenPack(v));
			it[2] = float(0.5f);
		}
	}
}

static bool
convert_file(const std::string& in_filename, const std::string& out_filename)
{
	ImageBuf inputImage(in_filename);
	bool ok = inputImage.read(0, 0, true, TypeDesc::FLOAT);

	if (!ok) {
		std::cerr << "convertNormalToQLog ERROR reading \"" << in_filename
			<< "\" : " << inputImage.geterror() << "\n";
	}

	convert_buffer(inputImage);

	ok = inputImage.write(out_filename);
	if (!ok)
	{
		std::cerr << "convertNormalToQLog ERROR writing \"" << out_filename
			<< "\" : " << inputImage.geterror() << "\n";
	}

	return ok;
}

int main(int argc, char* argv[])
{
	Filesystem::convert_native_arguments(argc, (const char**)argv);
	getargs(argc, argv);

	OIIO::attribute("threads", nthreads);

	bool ok = true;

	calculateApplyAndRemoveBias();

	ok = convert_file(filenames[0], filenames[1]);

	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
