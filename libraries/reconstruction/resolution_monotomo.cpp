/***************************************************************************
 *
 * Authors:    Jose Luis Vilas, 					  jlvilas@cnb.csic.es
 * 			   Carlos Oscar S. Sorzano            coss@cnb.csic.es (2016)
 *
 * Unidad de  Bioinformatica of Centro Nacional de Biotecnologia , CSIC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307  USA
 *
 *  All comments concerning this program package may be sent to the
 *  e-mail address 'xmipp@cnb.csic.es'
 ***************************************************************************/

#include "resolution_monotomo.h"
//#define DEBUG
//#define DEBUG_MASK

void ProgMonoTomo::readParams()
{
	fnVol = getParam("--vol");
	fnVol2 = getParam("--vol2");
	fnMeanVol = getParam("--meanVol");
	fnOut = getParam("-o");
	fnFilt = getParam("--filteredMap");
	fnMask = getParam("--mask");
	fnMaskOut = getParam("--mask_out");
	fnchim = getParam("--chimera_volume");
	sampling = getDoubleParam("--sampling_rate");
	R = getDoubleParam("--volumeRadius");
	minRes = getDoubleParam("--minRes");
	maxRes = getDoubleParam("--maxRes");
	freq_step = getDoubleParam("--step");
	trimBound = getDoubleParam("--trimmed");
	significance = getDoubleParam("--significance");
	fnMd = getParam("--md_outputdata");
	nthrs = getIntParam("--threads");
}


void ProgMonoTomo::defineParams()
{
	addUsageLine("This function determines the local resolution of a tomogram. It makes use of two reconstructions, odd and even. The difference between them"
			"gives a noise reconstruction. Thus, by computing the local amplitude of the signal at different frequencies and establishing a comparison with"
			"the noise, the local resolution is computed");
	addParamsLine("  --vol <vol_file=\"\">   			: Half volume 1");
	addParamsLine("  --vol2 <vol_file=\"\">				: Half volume 2");
	addParamsLine("  --mask <vol_file=\"\">  			: Mask defining the signal. ");
	addParamsLine("  [--mask_out <vol_file=\"\">]   	: Sometimes the provided mask is not perfect, and contains voxels out of the particle");
	addParamsLine("                          			:+ Thus the algorithm calculates a tight mask to the volume");
	addParamsLine("  -o <output=\"MGresolution.vol\">	: Local resolution volume (in Angstroms)");
	addParamsLine("  --filteredMap <output=\"filteredMap.vol\">	: Local resolution volume filtered (in Angstroms)");
	addParamsLine("  --meanVol <vol_file=\"\">			: Mean volume of half1 and half2 (only it is necessary the two haves are used)");
	addParamsLine("  [--chimera_volume <output=\"Chimera_resolution_volume.vol\">]: Local resolution volume for chimera viewer (in Angstroms)");
	addParamsLine("  [--sampling_rate <s=1>]   			: Sampling rate (A/px)");
	addParamsLine("  [--volumeRadius <s=100>]   		: This parameter determines the radius of a sphere where the volume is");
	addParamsLine("  [--step <s=0.25>]       			: The resolution is computed at a number of frequencies between minimum and");
	addParamsLine("                            			: maximum resolution px/A. This parameter determines that number");
	addParamsLine("  [--minRes <s=30>]         			: Minimum resolution (A)");
	addParamsLine("  [--maxRes <s=1>]         			: Maximum resolution (A)");
	addParamsLine("  [--trimmed <s=0.5>]       			: Trimming percentile");
	addParamsLine("  [--significance <s=0.95>]       	: The level of confidence for the hypothesis test.");
	addParamsLine("  [--md_outputdata <file=\".\">]  	: It is a control file. The provided mask can contain voxels of noise.");
	addParamsLine("                                  	: Moreover, voxels inside the mask cannot be measured due to an unsignificant");
	addParamsLine("                                  	: SNR. Thus, a new mask is created. This metadata file, shows, the number of");
	addParamsLine("                                  	: voxels of the original mask, and the created mask");
	addParamsLine("  [--threads <s=4>]               	: Number of threads");
}


void ProgMonoTomo::produceSideInfo()
{
	std::cout << "Starting..." << std::endl;
	Image<double> V;
	Image<double> V1, V2;
	V1.read(fnVol);
	V2.read(fnVol2);
	V()=0.5*(V1()+V2());
	V.write(fnMeanVol);

	V().setXmippOrigin();

	transformer_inv.setThreadsNumber(nthrs);

	FourierTransformer transformer;
	MultidimArray<double> &inputVol = V();
	VRiesz.resizeNoCopy(inputVol);

	transformer.FourierTransform(inputVol, fftV);
	iu.initZeros(fftV);

	// Calculate u and first component of Riesz vector
	double uz, uy, ux, uz2, u2, uz2y2;
	long n=0;
	for(size_t k=0; k<ZSIZE(fftV); ++k)
	{
		FFT_IDX2DIGFREQ(k,ZSIZE(inputVol),uz);
		uz2=uz*uz;

		for(size_t i=0; i<YSIZE(fftV); ++i)
		{
			FFT_IDX2DIGFREQ(i,YSIZE(inputVol),uy);
			uz2y2=uz2+uy*uy;

			for(size_t j=0; j<XSIZE(fftV); ++j)
			{
				FFT_IDX2DIGFREQ(j,XSIZE(inputVol),ux);
				u2=uz2y2+ux*ux;
				if ((k != 0) || (i != 0) || (j != 0))
					DIRECT_MULTIDIM_ELEM(iu,n) = 1.0/sqrt(u2);
				else
					DIRECT_MULTIDIM_ELEM(iu,n) = 1e38;
				++n;
			}
		}
	}
	#ifdef DEBUG
	Image<double> saveiu;
	saveiu = 1/iu;
	saveiu.write("iu.vol");
	#endif

	// Prepare low pass filter
	lowPassFilter.FilterShape = RAISED_COSINE;
	lowPassFilter.raised_w = 0.01;
	lowPassFilter.do_generate_3dmask = false;
	lowPassFilter.FilterBand = LOWPASS;

	// Prepare mask
	MultidimArray<int> &pMask=mask();

	if (fnMask != "")
	{
		mask.read(fnMask);
		mask().setXmippOrigin();
	}
	else
	{
		std::cout << "Error: a mask ought to be provided" << std::endl;
		exit(0);
	}

	NVoxelsOriginalMask = 0;

	FOR_ALL_ELEMENTS_IN_ARRAY3D(pMask)
	{
		if (A3D_ELEM(pMask, k, i, j) == 1)
			++NVoxelsOriginalMask;
//		if (i*i+j*j+k*k > R*R)
//			A3D_ELEM(pMask, k, i, j) = -1;
	}

//	#ifdef DEBUG_MASK
	mask.write("mask.vol");
//	#endif


	V1.read(fnVol);
	V2.read(fnVol2);

	V1()-=V2();
	V1()/=2;
	fftN=new MultidimArray< std::complex<double> >;
	FourierTransformer transformer2;
	#ifdef DEBUG
	  V1.write("diff_volume.vol");
	#endif

	int N_smoothing = 10;

	// -50...49
	int siz_z = ZSIZE(inputVol)*0.5;
	int siz_y = YSIZE(inputVol)*0.5;
	int siz_x = XSIZE(inputVol)*0.5;

//	std::cout << "XSIZE(inputVol) = " << XSIZE(inputVol) << std::endl;
//	std::cout << "YSIZE(inputVol) = " << YSIZE(inputVol) << std::endl;
//	std::cout << "ZSIZE(inputVol) = " << ZSIZE(inputVol) << std::endl;

	int limit_distance_x = (siz_x-N_smoothing);
	int limit_distance_y = (siz_y-N_smoothing);
	int limit_distance_z = (siz_z-N_smoothing);

//	std::cout << "limit_distance_x = " << limit_distance_x << std::endl;
//	std::cout << "limit_distance_y = " << limit_distance_y << std::endl;
//	std::cout << "limit_distance_z = " << limit_distance_z << std::endl;

	n=0;
	for(int k=0; k<ZSIZE(inputVol); ++k)
	{
		uz = (k - siz_z);
		for(int i=0; i<YSIZE(inputVol); ++i)
		{
			uy = (i - siz_y);
			for(int j=0; j<XSIZE(inputVol); ++j)
			{
				ux = (j - siz_x);

				if (abs(ux)>=limit_distance_x)
				{
					DIRECT_MULTIDIM_ELEM(V1(), n) *= 0.5*(1+cos(PI*(limit_distance_x - abs(ux))/(N_smoothing)));
					DIRECT_MULTIDIM_ELEM(pMask, n) = 0;
				}

				if (abs(uy)>=limit_distance_y)
				{
					DIRECT_MULTIDIM_ELEM(V1(), n) *= 0.5*(1+cos(PI*(limit_distance_y - abs(uy))/(N_smoothing)));
					DIRECT_MULTIDIM_ELEM(pMask, n) = 0;
				}
				if (abs(uz)>=limit_distance_z)
				{
					DIRECT_MULTIDIM_ELEM(V1(), n) *= 0.5*(1+cos(PI*(limit_distance_z - abs(uz))/(N_smoothing)));
					DIRECT_MULTIDIM_ELEM(pMask, n) = 0;
				}
				++n;
			}
		}
	}

	Image<double> filteredvolume;
	filteredvolume = V1();

//	Matrix2D<double> noiseMatrix;
//
//	localNoise(filteredvolume(), noiseMatrix);
//	exit(0);

	filteredvolume.write(formatString("Volumen_suavizado.vol"));

	transformer2.FourierTransform(V1(), *fftN);

	V.clear();

	double u;

	freq_fourier_z.initZeros(ZSIZE(fftV));
	freq_fourier_x.initZeros(XSIZE(fftV));
	freq_fourier_y.initZeros(YSIZE(fftV));

	//TODO: check if the frequency assignmen is right with the mask

	VEC_ELEM(freq_fourier_z,0) = 1e-38;
	for(size_t k=0; k<ZSIZE(fftV); ++k)
	{
		FFT_IDX2DIGFREQ(k,ZSIZE(pMask), u);
		VEC_ELEM(freq_fourier_z,k) = u;
	}

	VEC_ELEM(freq_fourier_y,0) = 1e-38;
	for(size_t k=0; k<YSIZE(fftV); ++k)
	{
		FFT_IDX2DIGFREQ(k,YSIZE(pMask), u);
		VEC_ELEM(freq_fourier_y,k) = u;
	}

	VEC_ELEM(freq_fourier_x,0) = 1e-38;
	for(size_t k=0; k<XSIZE(fftV); ++k)
	{
		FFT_IDX2DIGFREQ(k,XSIZE(pMask), u);
		VEC_ELEM(freq_fourier_x,k) = u;
	}

}


void ProgMonoTomo::amplitudeMonogenicSignal3D(MultidimArray< std::complex<double> > &myfftV,
		double freq, double freqH, double freqL, MultidimArray<double> &amplitude, int count, FileName fnDebug)
{
	fftVRiesz.initZeros(myfftV);
	fftVRiesz_aux.initZeros(myfftV);
	std::complex<double> J(0,1);

	// Filter the input volume and add it to amplitude
	long n=0;
	double ideltal=PI/(freq-freqH);

	FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(myfftV)
	{
		double iun=DIRECT_MULTIDIM_ELEM(iu,n);
		double un=1.0/iun;
		if (freqH<=un && un<=freq)
		{
			//double H=0.5*(1+cos((un-w1)*ideltal));
			DIRECT_MULTIDIM_ELEM(fftVRiesz, n) = DIRECT_MULTIDIM_ELEM(myfftV, n);
			DIRECT_MULTIDIM_ELEM(fftVRiesz, n) *= 0.5*(1+cos((un-freq)*ideltal));//H;
			DIRECT_MULTIDIM_ELEM(fftVRiesz_aux, n) = -J;
			DIRECT_MULTIDIM_ELEM(fftVRiesz_aux, n) *= DIRECT_MULTIDIM_ELEM(fftVRiesz, n);
			DIRECT_MULTIDIM_ELEM(fftVRiesz_aux, n) *= iun;
		} else if (un>freq)
		{
			DIRECT_MULTIDIM_ELEM(fftVRiesz, n) = DIRECT_MULTIDIM_ELEM(myfftV, n);
			DIRECT_MULTIDIM_ELEM(fftVRiesz_aux, n) = -J;
			DIRECT_MULTIDIM_ELEM(fftVRiesz_aux, n) *= DIRECT_MULTIDIM_ELEM(fftVRiesz, n);
			DIRECT_MULTIDIM_ELEM(fftVRiesz_aux, n) *= iun;
		}
	}

	transformer_inv.inverseFourierTransform(fftVRiesz, VRiesz);

	#ifdef DEBUG
	Image<double> filteredvolume;
	filteredvolume = VRiesz;
	filteredvolume.write(formatString("Volumen_filtrado_%i.vol", count));

	FileName iternumber;
	iternumber = formatString("_Volume_%i.vol", count);
	Image<double> saveImg2;
	saveImg2() = VRiesz;
	  if (fnDebug.c_str() != "")
	  {
		saveImg2.write(fnDebug+iternumber);
	  }
	saveImg2.clear(); 
	#endif

	amplitude.resizeNoCopy(VRiesz);

	FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(amplitude)
		DIRECT_MULTIDIM_ELEM(amplitude,n)=DIRECT_MULTIDIM_ELEM(VRiesz,n)*DIRECT_MULTIDIM_ELEM(VRiesz,n);

	// Calculate first component of Riesz vector
	double uz, uy, ux;
	n=0;
	for(size_t k=0; k<ZSIZE(myfftV); ++k)
	{
		for(size_t i=0; i<YSIZE(myfftV); ++i)
		{
			for(size_t j=0; j<XSIZE(myfftV); ++j)
			{
				ux = VEC_ELEM(freq_fourier_x,j);
				DIRECT_MULTIDIM_ELEM(fftVRiesz, n) = ux*DIRECT_MULTIDIM_ELEM(fftVRiesz_aux, n);
				++n;
			}
		}
	}
	transformer_inv.inverseFourierTransform(fftVRiesz, VRiesz);
	FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(amplitude)
		DIRECT_MULTIDIM_ELEM(amplitude,n)+=DIRECT_MULTIDIM_ELEM(VRiesz,n)*DIRECT_MULTIDIM_ELEM(VRiesz,n);

	// Calculate second and third components of Riesz vector
	n=0;
	for(size_t k=0; k<ZSIZE(myfftV); ++k)
	{
		uz = VEC_ELEM(freq_fourier_z,k);
		for(size_t i=0; i<YSIZE(myfftV); ++i)
		{
			uy = VEC_ELEM(freq_fourier_y,i);
			for(size_t j=0; j<XSIZE(myfftV); ++j)
			{
				DIRECT_MULTIDIM_ELEM(fftVRiesz, n) = uy*DIRECT_MULTIDIM_ELEM(fftVRiesz_aux, n);
				DIRECT_MULTIDIM_ELEM(fftVRiesz_aux, n) = uz*DIRECT_MULTIDIM_ELEM(fftVRiesz_aux, n);
				++n;
			}
		}
	}
	transformer_inv.inverseFourierTransform(fftVRiesz, VRiesz);

	FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(amplitude)
		DIRECT_MULTIDIM_ELEM(amplitude,n)+=DIRECT_MULTIDIM_ELEM(VRiesz,n)*DIRECT_MULTIDIM_ELEM(VRiesz,n);

	transformer_inv.inverseFourierTransform(fftVRiesz_aux, VRiesz);

	FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(amplitude)
	{
		DIRECT_MULTIDIM_ELEM(amplitude,n)+=DIRECT_MULTIDIM_ELEM(VRiesz,n)*DIRECT_MULTIDIM_ELEM(VRiesz,n);
		DIRECT_MULTIDIM_ELEM(amplitude,n)=sqrt(DIRECT_MULTIDIM_ELEM(amplitude,n));
	}
//	#ifdef DEBUG
//	if (fnDebug.c_str() != "")
//	{
//	Image<double> saveImg;
//	saveImg = amplitude;
//	FileName iternumber;
//	iternumber = formatString("_Amplitude_%i.vol", count);
//	saveImg.write(fnDebug+iternumber);
//	saveImg.clear();
//	}
//	#endif // DEBUG
//
	// Low pass filter the monogenic amplitude
	transformer_inv.FourierTransform(amplitude, fftVRiesz, false);
	double raised_w = PI/(freqL-freq);

	n=0;

	FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(fftVRiesz)
	{
		double un=1.0/DIRECT_MULTIDIM_ELEM(iu,n);
		if ((freqL)>=un && un>=freq)
		{
			DIRECT_MULTIDIM_ELEM(fftVRiesz,n) *= 0.5*(1 + cos(raised_w*(un-freq)));
		}
		else
		{
			if (un>freqL)
			{
				DIRECT_MULTIDIM_ELEM(fftVRiesz,n) = 0;
			}
		}
	}
	transformer_inv.inverseFourierTransform();


//	#ifdef DEBUG
//	Image<double> saveImg2;
//	saveImg2 = amplitude;
//	FileName fnSaveImg2;
//	if (fnDebug.c_str() != "")
//	{
//		iternumber = formatString("_Filtered_Amplitude_%i.vol", count);
//		saveImg2.write(fnDebug+iternumber);
//	}
//	saveImg2.clear();
//	#endif // DEBUG
}


void ProgMonoTomo::median3x3x3(MultidimArray<double> vol, MultidimArray<double> &filtered)
{
	std::vector<double> list;
	double truevalue;
	FOR_ALL_ELEMENTS_IN_ARRAY3D(vol)
	{
		if ((k>0) && (i>0) && (j>0))
		{
		list.push_back(A3D_ELEM(vol, k, i, j));
		list.push_back(A3D_ELEM(vol, k, i, j-1));
		list.push_back(A3D_ELEM(vol, k, i-1, j));
		list.push_back(A3D_ELEM(vol, k, i-1, j-1));
		list.push_back(A3D_ELEM(vol, k-1, i, j));
		list.push_back(A3D_ELEM(vol, k-1, i, j-1));
		list.push_back(A3D_ELEM(vol, k-1, i-1, j));
		list.push_back(A3D_ELEM(vol, k-1, i-1, j-1));
		list.push_back(A3D_ELEM(vol, k, i, j));
		list.push_back(A3D_ELEM(vol, k, i, j+1));
		list.push_back(A3D_ELEM(vol, k, i+1, j));
		list.push_back(A3D_ELEM(vol, k, i+1, j+1));
		list.push_back(A3D_ELEM(vol, k+1, i, j));
		list.push_back(A3D_ELEM(vol, k+1, i, j+1));
		list.push_back(A3D_ELEM(vol, k+1, i+1, j));
		list.push_back(A3D_ELEM(vol, k+1, i+1, j+1));

		std::sort(list.begin(),list.end());
		truevalue = list[size_t(list.size()*0.5)];

		list.clear();

		A3D_ELEM(filtered, k, i, j) = truevalue;

		}
	}
}


void ProgMonoTomo::localNoise(MultidimArray<double> &noiseMap, Matrix2D<double> &noiseMatrix, int boxsize)
{
//	std::cout << "Analyzing local noise" << std::endl;

	//TODO: check this function
	double kk = 0;
	int idx_i, idx_j;

	int xdim = XSIZE(noiseMap);
	int ydim = YSIZE(noiseMap);

//	noiseMap.printShape();
//
//	noiseMap.printShape();

	int Nx = xdim/boxsize;
	int Ny = ydim/boxsize;

//	std::cout << "xdim = " << xdim << std::endl;
//
//	std::cout << "Nx = " << Nx << std::endl;
//	std::cout << "Ny = " << Ny << std::endl;

	STARTINGX(noiseMap);

	noiseMatrix.initZeros(Nx, Ny);

	if ( (xdim<boxsize) || (ydim<boxsize) )
		std::cout << "Error: The tomogram in x-direction or y-direction is too small" << std::endl;

//	std::cout << "STARTINGX(noiseMap) " << STARTINGX(noiseMap) << " " << FINISHINGX(noiseMap) << std::endl;
//	std::cout << "STARTINGY(noiseMap) " << STARTINGY(noiseMap) << " " << FINISHINGY(noiseMap) << std::endl;

	int step = 1000;
	std::vector<double> noiseVector;

	noiseVector.resize(step,0);

	int halfsize_x = 0.5*boxsize;
	int halfsize_y = 0.5*boxsize;
	int halfsize_z = 0.5*ZSIZE(noiseMap);

	int xLimit, yLimit, xStart, yStart;

	MultidimArray<double> piece;

	int X_boxIdx=0;
	int Y_boxIdx=0;

//	// El problema es el boxsize, en particular en el eje z
//	for (int k = STARTINGZ(noiseMap)+halfsize_z; k<=FINISHINGZ(noiseMap)-halfsize_z; k+=ZSIZE(noiseMap))
//	{
//		for (int i = STARTINGY(noiseMap)+halfsize_y; i<=FINISHINGY(noiseMap)-halfsize_y; i+=boxsize)
//		{
//			if (X_boxIdx==Nx-1)
//				boxsize = FINISHINGX(noiseMap);
//
//			for (int j = STARTINGX(noiseMap)+halfsize_x; j<=FINISHINGX(noiseMap)-halfsize_x; j+=boxsize)
//			{
//				noiseMap.windowSameTypeInside(piece, k-halfsize_z, i-halfsize_y, j-halfsize_x, k+halfsize_z, i+halfsize_y, j+halfsize_x);
//
//				int idx_vector = 0;
//				for (long n = 0; n<MULTIDIM_SIZE(piece); n+= MULTIDIM_SIZE(piece)/step)
//				{
//					noiseVector[idx_vector] = DIRECT_MULTIDIM_ELEM(piece,n);
//					++idx_vector;
//				}
//
//				std::sort(noiseVector.begin(),noiseVector.end());
//				MAT_ELEM(noiseMatrix, X_boxIdx, Y_boxIdx) = noiseVector[step*significance];
//
//				++Y_boxIdx;
//			}
//			++X_boxIdx;
//		}
//	}

	long counter;

	for (int X_boxIdx=0; X_boxIdx<Nx; ++X_boxIdx)
	{
		if (X_boxIdx==Nx-1)
		{
			xStart = STARTINGX(noiseMap) + X_boxIdx*boxsize;
			xLimit = FINISHINGX(noiseMap);
		}
		else
		{
			xStart = STARTINGX(noiseMap) + X_boxIdx*boxsize;
			xLimit = STARTINGX(noiseMap) + (X_boxIdx+1)*boxsize;
		}

		for (int Y_boxIdx=0; Y_boxIdx<Ny; ++Y_boxIdx)
		{
			if (Y_boxIdx==Ny-1)
			{
				yStart = STARTINGY(noiseMap) + Y_boxIdx*boxsize;
				yLimit =  FINISHINGY(noiseMap);
			}
			else
			{
				yStart = STARTINGY(noiseMap) + Y_boxIdx*boxsize;
				yLimit = STARTINGY(noiseMap) + (Y_boxIdx+1)*boxsize;
			}

			counter = 0;
			for (int i = (yStart); i<(yLimit); i++)
			{
				for (int j = (xStart); j<(xLimit); j++)
				{
					for (int k =STARTINGZ(noiseMap); k<FINISHINGZ(noiseMap); k++)
					{
						if (counter%1000 == 0)
						{
						noiseVector.push_back( A3D_ELEM(noiseMap, k, i, j) );
						}
						++counter;
					}
				}
			}

			std::sort(noiseVector.begin(),noiseVector.end());
			MAT_ELEM(noiseMatrix,X_boxIdx, Y_boxIdx) = noiseVector[size_t(noiseVector.size()*significance)];

			noiseVector.clear();
		}
	}
//	std::cout << "MatrixThreshold = " << MAT_ELEM(noiseMatrix, 1,0) << std::endl;

//	std::cout << "MatrixThreshold = " << noiseMatrix << std::endl;
}


void ProgMonoTomo::postProcessingLocalResolutions(MultidimArray<double> &resolutionVol,
		std::vector<double> &list, MultidimArray<double> &resolutionChimera, double &cut_value, MultidimArray<int> &pMask, double &resolutionThreshold)
{
	MultidimArray<double> resolutionVol_aux = resolutionVol;
	double last_resolution_2 = list[(list.size()-1)];

	double Nyquist;
	Nyquist = 2*sampling;


	double lowest_res;
	lowest_res = list[0];

	// Count number of voxels with resolution
	size_t N=0;
	FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(resolutionVol)
		if ( (DIRECT_MULTIDIM_ELEM(resolutionVol, n)>=(last_resolution_2-0.001)) && (DIRECT_MULTIDIM_ELEM(resolutionVol, n)<lowest_res) ) //the value 0.001 is a tolerance
			++N;

	// Get all resolution values
	MultidimArray<double> resolutions(N);
	size_t N_iter=0;

	FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(resolutionVol)
		if ( (DIRECT_MULTIDIM_ELEM(resolutionVol, n)>(last_resolution_2-0.001)) && (DIRECT_MULTIDIM_ELEM(resolutionVol, n)<lowest_res))
		{
			DIRECT_MULTIDIM_ELEM(resolutions,N_iter++)=DIRECT_MULTIDIM_ELEM(resolutionVol, n);
		}

//	median = resolutionVector[size_t(resolutionVector.size()*0.5)];

	// Sort value and get threshold
	std::sort(&A1D_ELEM(resolutions,0),&A1D_ELEM(resolutions,N));
	double filling_value = A1D_ELEM(resolutions, (int)(0.5*N)); //median value
	double trimming_value = A1D_ELEM(resolutions, (int)((1-cut_value)*N));

	std::cout << "last_res = " << filling_value << std::endl;
	std::cout << "last_res = " << trimming_value << std::endl;

	double init_res, last_res;

	init_res = list[0];
	last_res = list[(list.size()-1)];
	
	std::cout << "--------------------------" << std::endl;
	std::cout << "last_res = " << last_res << std::endl;

	resolutionChimera = resolutionVol;


	FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(resolutionVol)
	{
		if (DIRECT_MULTIDIM_ELEM(resolutionVol, n) < last_res)
		{
			if (DIRECT_MULTIDIM_ELEM(pMask, n) >=1)
			{
				DIRECT_MULTIDIM_ELEM(resolutionChimera, n) = filling_value;
				DIRECT_MULTIDIM_ELEM(resolutionVol, n) = filling_value;
			}
			else
			{
				DIRECT_MULTIDIM_ELEM(resolutionChimera, n) = filling_value;
				DIRECT_MULTIDIM_ELEM(pMask,n) = 0;
			}
		}
		if (DIRECT_MULTIDIM_ELEM(resolutionVol, n) > trimming_value)
		{
		  DIRECT_MULTIDIM_ELEM(pMask,n) = 0;
		  DIRECT_MULTIDIM_ELEM(resolutionVol, n) = trimming_value;
		  DIRECT_MULTIDIM_ELEM(resolutionChimera, n) = trimming_value;
		}
	}

	N=0;
	FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(resolutionVol)
	if (DIRECT_MULTIDIM_ELEM(pMask, n) >=1)
		{
		if (DIRECT_MULTIDIM_ELEM(resolutionVol, n)>=(last_resolution_2-0.001)) //the value 0.001 is a tolerance
			++N;
		}

	MultidimArray<double> resolutions2(N);
	N_iter=0;

	FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(resolutionVol)
	{
		if (DIRECT_MULTIDIM_ELEM(pMask, n) >=1)
			{
			if (DIRECT_MULTIDIM_ELEM(resolutionVol, n)>(last_resolution_2-0.001))
				DIRECT_MULTIDIM_ELEM(resolutions2,N_iter++)=DIRECT_MULTIDIM_ELEM(resolutionVol, n);
			}
	}

	// Sort value and get threshold
	std::sort(&A1D_ELEM(resolutions2,0),&A1D_ELEM(resolutions2,N));
	std::cout << "median Resolution = " << A1D_ELEM(resolutions2, (int)(0.5*N)) << std::endl;


	Image<int> imgMask;
	imgMask = pMask;
	imgMask.write(fnMaskOut);
}


void ProgMonoTomo::lowestResolutionbyPercentile(MultidimArray<double> &resolutionVol,
		std::vector<double> &list, double &cut_value, double &resolutionThreshold)
{
	double last_resolution_2 = list[(list.size()-1)];

	double Nyquist;
	Nyquist = 2*sampling;

	double lowest_res;
	lowest_res = list[0];

	// Count number of voxels with resolution
	size_t N=0;
	FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(resolutionVol)
		if ( (DIRECT_MULTIDIM_ELEM(resolutionVol, n)>=(last_resolution_2-0.001)) )//&& (DIRECT_MULTIDIM_ELEM(resolutionVol, n)<lowest_res) ) //the value 0.001 is a tolerance
			++N;

	// Get all resolution values
	MultidimArray<double> resolutions(N);
	size_t N_iter=0;

	FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(resolutionVol)
		if ( (DIRECT_MULTIDIM_ELEM(resolutionVol, n)>(last_resolution_2-0.001)) )//&& (DIRECT_MULTIDIM_ELEM(resolutionVol, n)<lowest_res))
		{
			DIRECT_MULTIDIM_ELEM(resolutions,N_iter++)=DIRECT_MULTIDIM_ELEM(resolutionVol, n);
		}

//	median = resolutionVector[size_t(resolutionVector.size()*0.5)];

	// Sort value and get threshold
	std::sort(&A1D_ELEM(resolutions,0),&A1D_ELEM(resolutions,N));
	double filling_value = A1D_ELEM(resolutions, (int)(0.5*N)); //median value
	double trimming_value = A1D_ELEM(resolutions, (int)((1-cut_value)*N));
	resolutionThreshold = A1D_ELEM(resolutions, (int)((0.95)*N));

	//std::cout << "resolutionThreshold = " << resolutionThreshold <<  std::endl;
}


void ProgMonoTomo::resolution2eval(int &count_res, double step,
								double &resolution, double &last_resolution,
								double &freq, double &freqL,
								int &last_fourier_idx,
								bool &continueIter,	bool &breakIter,
								bool &doNextIteration)
{
	resolution = maxRes - count_res*step;
	freq = sampling/resolution;
	++count_res;

	double Nyquist = 2*sampling;
	double aux_frequency;
	int fourier_idx;

	DIGFREQ2FFT_IDX(freq, ZSIZE(VRiesz), fourier_idx);

	FFT_IDX2DIGFREQ(fourier_idx, ZSIZE(VRiesz), aux_frequency);

	freq = aux_frequency;

	if (fourier_idx == last_fourier_idx)
	{
		continueIter = true;
		return;
	}

	last_fourier_idx = fourier_idx;
	resolution = sampling/aux_frequency;


	if (count_res == 0)
		last_resolution = resolution;

	if ( ( resolution<Nyquist ))
	{
		breakIter = true;
		return;
	}

	freqL = sampling/(resolution + step);

	int fourier_idx_2;

	DIGFREQ2FFT_IDX(freqL, ZSIZE(VRiesz), fourier_idx_2);

	if (fourier_idx_2 == fourier_idx)
	{
		if (fourier_idx > 0){
			FFT_IDX2DIGFREQ(fourier_idx - 1, ZSIZE(VRiesz), freqL);
		}
		else{
			freqL = sampling/(resolution + step);
		}
	}

}




void ProgMonoTomo::run()
{
	produceSideInfo();

	Image<double> outputResolution;
	outputResolution().initZeros(VRiesz);

	MultidimArray<int> &pMask = mask();
	MultidimArray<double> &pOutputResolution = outputResolution();
	MultidimArray<double> &pVfiltered = Vfiltered();
	MultidimArray<double> &pVresolutionFiltered = VresolutionFiltered();
	MultidimArray<double> amplitudeMS, amplitudeMN;

	std::cout << "Looking for maximum frequency ..." << std::endl;
	double criticalZ=icdf_gauss(significance);
	double criticalW=-1;
	double resolution, resolution_2, last_resolution = 10000;  //A huge value for achieving
												//last_resolution < resolution
	double freq, freqH, freqL;
	double max_meanS = -1e38;
	double cut_value = 0.025;
	int boxsize = 100;


//	Image<int> imgMask2;
//	imgMask2 = mask;
//	imgMask2.write("mascara_original.vol");

	double R_ = freq_step;

	if (R_<0.25)
		R_=0.25;

	double Nyquist = 2*sampling;
	if (minRes<2*sampling)
		minRes = Nyquist;

	bool doNextIteration=true;

	bool lefttrimming = false;
	int last_fourier_idx = -1;

	int count_res = 0;
	FileName fnDebug;

	int iter=0;
	std::vector<double> list;

	std::cout << "Analyzing frequencies" << std::endl;
	std::vector<double> noiseValues;

	do
	{
		bool continueIter = false;
		bool breakIter = false;

		resolution2eval(count_res, R_,
						resolution, last_resolution,
						freq, freqH,
						last_fourier_idx, continueIter, breakIter, doNextIteration);

		if (continueIter)
			continue;

		if (breakIter)
			break;

		std::cout << "resolution = " << resolution << std::endl;


		list.push_back(resolution);

		if (iter <2)
			resolution_2 = list[0];
		else
			resolution_2 = list[iter - 2];

		fnDebug = "Signal";

		freqL = freq + 0.01;
//		if (freqL>=0.5)
//			freqL = 0.5;

		amplitudeMonogenicSignal3D(fftV, freq, freqH, freqL, amplitudeMS, iter, fnDebug);
		fnDebug = "Noise";
		amplitudeMonogenicSignal3D(*fftN, freq, freqH, freqL, amplitudeMN, iter, fnDebug);

		Matrix2D<double> noiseMatrix;

		localNoise(amplitudeMN, noiseMatrix, boxsize);


		double sumS=0, sumS2=0, sumN=0, sumN2=0, NN = 0, NS = 0;
		noiseValues.clear();


		FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(amplitudeMS)
		{
			if (DIRECT_MULTIDIM_ELEM(pMask, n)>=1)
			{
				double amplitudeValue=DIRECT_MULTIDIM_ELEM(amplitudeMS, n);
				double amplitudeValueN=DIRECT_MULTIDIM_ELEM(amplitudeMN, n);
				sumS  += amplitudeValue;
				sumS2 += amplitudeValue*amplitudeValue;
				noiseValues.push_back(amplitudeValueN);
				sumN  += amplitudeValueN;
				sumN2 += amplitudeValueN*amplitudeValueN;
				++NS;
				++NN;
			}
		}

	
		#ifdef DEBUG
		std::cout << "NS" << NS << std::endl;
		std::cout << "NVoxelsOriginalMask" << NVoxelsOriginalMask << std::endl;
		std::cout << "NS/NVoxelsOriginalMask = " << NS/NVoxelsOriginalMask << std::endl;
		#endif
		

			if (NS == 0)
			{
				std::cout << "There are no points to compute inside the mask" << std::endl;
				std::cout << "If the number of computed frequencies is low, perhaps the provided"
						"mask is not enough tight to the volume, in that case please try another mask" << std::endl;
				break;
			}

			double meanS=sumS/NS;
			double sigma2S=sumS2/NS-meanS*meanS;
			double meanN=sumN/NN;
			double sigma2N=sumN2/NN-meanN*meanN;

			// Check local resolution
//			double thresholdNoise;
//			std::sort(noiseValues.begin(),noiseValues.end());
//			thresholdNoise = noiseValues[size_t(noiseValues.size()*significance)];
//			std::cout << "thr Noise " << thresholdNoise << std::endl;


			#ifdef DEBUG
			  std::cout << "Iteration = " << iter << ",   Resolution= " << resolution <<
					  ",   Signal = " << meanS << ",   Noise = " << meanN << ",  Threshold = "
					  << thresholdNoise <<std::endl;
			#endif

			double z=(meanS-meanN)/sqrt(sigma2S/NS+sigma2N/NN);

//			std::cout << "z = " << z << "  zcritical = " << criticalZ << std::endl;

			if (meanS>max_meanS)
				max_meanS = meanS;

			if (meanS<0.001*max_meanS)
			{
				std::cout << "Search of resolutions stopped due to too low signal" << std::endl;
				break;
			}

//			pMask.printShape();
//			amplitudeMS.printShape();
//			pOutputResolution.printShape();

			FOR_ALL_DIRECT_ELEMENTS_IN_ARRAY3D(amplitudeMS)
			{
				if (DIRECT_A3D_ELEM(pMask, k,i,j)>=1)
				{
					size_t idx_x = (i/boxsize);
					size_t idx_y = (j/boxsize);

					if (idx_x==9)
						idx_x= 8;
					if (idx_y==9)
						idx_y= 8;

					if ( DIRECT_A3D_ELEM(amplitudeMS, k,i,j)>MAT_ELEM(noiseMatrix, idx_x, idx_y) )
					{

						DIRECT_A3D_ELEM(pMask,  k,i,j) = 1;
						DIRECT_A3D_ELEM(pOutputResolution, k,i,j) = resolution;//sampling/freq;
					}
					else{
						DIRECT_A3D_ELEM(pMask,  k,i,j) += 1;
						if (DIRECT_A3D_ELEM(pMask,  k,i,j) >2)
						{
							DIRECT_A3D_ELEM(pMask,  k,i,j) = -1;
							DIRECT_A3D_ELEM(pOutputResolution,  k,i,j) = resolution_2;//maxRes - counter*R_;
						}
					}
				}
			}
//
//			FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(amplitudeMS)
//			{
//				if (DIRECT_MULTIDIM_ELEM(pMask, n)>=1)
//					if (DIRECT_MULTIDIM_ELEM(amplitudeMS, n)>thresholdNoise)
//					{
//						DIRECT_MULTIDIM_ELEM(pMask, n) = 1;
//						DIRECT_MULTIDIM_ELEM(pOutputResolution, n) = resolution;//sampling/freq;
//					}
//					else{
//						DIRECT_MULTIDIM_ELEM(pMask, n) += 1;
//						if (DIRECT_MULTIDIM_ELEM(pMask, n) >2)
//						{
//							DIRECT_MULTIDIM_ELEM(pMask, n) = -1;
//							DIRECT_MULTIDIM_ELEM(pOutputResolution, n) = resolution_2;//maxRes - counter*R_;
//						}
//					}
//			}




			#ifdef DEBUG_MASK
			FileName fnmask_debug;
			fnmask_debug = formatString("maske_%i.vol", iter);
			mask.write(fnmask_debug);
			#endif

			// Is the mean inside the signal significantly different from the noise?

			/*z=(meanS-meanN)/sqrt(sigma2S/NS+sigma2N/NN);
			 *

			#ifdef DEBUG
				std::cout << "thresholdNoise = " << thresholdNoise << std::endl;
				std::cout << "  meanS= " << meanS << " sigma2S= " << sigma2S << " NS= " << NS << std::endl;
				std::cout << "  meanN= " << meanN << " sigma2N= " << sigma2N << " NN= " << NN << std::endl;
				std::cout << "  z=" << z << " (" << criticalZ << ")" << std::endl;
			#endif
			if (z<criticalZ)
			{
				criticalW = freq;
				std::cout << "Search stopped due to z>Z (hypothesis test)" << std::endl;
				doNextIteration=false;
			}
			*/
			if (doNextIteration)
			{
				if (resolution <= (minRes-0.001))
					doNextIteration = false;
			}

//		}
		iter++;
		last_resolution = resolution;
	} while (doNextIteration);

	//TODO: Check if this condition is useful

	if (lefttrimming == false)
	{
	  Nvoxels = 0;
	  FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(amplitudeMS)
	  {
	    if (DIRECT_MULTIDIM_ELEM(pOutputResolution, n) == 0)
	      DIRECT_MULTIDIM_ELEM(pMask, n) = 0;
	    else
	    {
	      Nvoxels++;
	      DIRECT_MULTIDIM_ELEM(pMask, n) = 1;
	    }
	  }

	}
	amplitudeMN.clear();
	amplitudeMS.clear();



	//Convolution with a real gaussian to get a smooth map
	MultidimArray<double> FilteredResolution = pOutputResolution;
	double sigma = 30.0;

	realGaussianFilter(FilteredResolution, sigma);

	double resolutionThreshold;
	lowestResolutionbyPercentile(FilteredResolution, list, cut_value, resolutionThreshold);



	FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(FilteredResolution)
	{
		if ( (DIRECT_MULTIDIM_ELEM(FilteredResolution, n)<resolutionThreshold) && (DIRECT_MULTIDIM_ELEM(FilteredResolution, n)>DIRECT_MULTIDIM_ELEM(pOutputResolution, n)) )
			DIRECT_MULTIDIM_ELEM(FilteredResolution, n) = DIRECT_MULTIDIM_ELEM(pOutputResolution, n);

		if ( DIRECT_MULTIDIM_ELEM(FilteredResolution, n)<Nyquist)
			DIRECT_MULTIDIM_ELEM(FilteredResolution, n) = Nyquist;
	}

	realGaussianFilter(FilteredResolution, sigma);

	Image<double> outputResolutionImage;
	outputResolutionImage() = FilteredResolution;
	outputResolutionImage.write(fnFilt);

	FilteredResolution = pOutputResolution;
	sigma = 3;

	realGaussianFilter(FilteredResolution, sigma);


	lowestResolutionbyPercentile(FilteredResolution, list, cut_value, resolutionThreshold);



	FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(FilteredResolution)
	{
		if ( (DIRECT_MULTIDIM_ELEM(FilteredResolution, n)<resolutionThreshold) && (DIRECT_MULTIDIM_ELEM(FilteredResolution, n)>DIRECT_MULTIDIM_ELEM(pOutputResolution, n)) )
			DIRECT_MULTIDIM_ELEM(FilteredResolution, n) = DIRECT_MULTIDIM_ELEM(pOutputResolution, n);
		if ( DIRECT_MULTIDIM_ELEM(FilteredResolution, n)<Nyquist)
			DIRECT_MULTIDIM_ELEM(FilteredResolution, n) = Nyquist;
	}

	realGaussianFilter(FilteredResolution, sigma);


	FOR_ALL_DIRECT_ELEMENTS_IN_MULTIDIMARRAY(FilteredResolution)
	{
		if ((DIRECT_MULTIDIM_ELEM(FilteredResolution, n)<resolutionThreshold)  && (DIRECT_MULTIDIM_ELEM(FilteredResolution, n)>DIRECT_MULTIDIM_ELEM(pOutputResolution, n)))
			DIRECT_MULTIDIM_ELEM(FilteredResolution, n) = DIRECT_MULTIDIM_ELEM(pOutputResolution, n);
		if ( DIRECT_MULTIDIM_ELEM(FilteredResolution, n)<Nyquist)
			DIRECT_MULTIDIM_ELEM(FilteredResolution, n) = Nyquist;
	}

//	pOutputResolution = FilteredResolution;

	MultidimArray<double> resolutionFiltered, resolutionChimera;

//	postProcessingLocalResolutions(pOutputResolution, list, resolutionChimera, cut_value, pMask, resolutionThreshold);
	postProcessingLocalResolutions(FilteredResolution, list, resolutionChimera, cut_value, pMask, resolutionThreshold);


//	outputResolutionImage() = pOutputResolution;//resolutionFiltered;
	outputResolutionImage() = FilteredResolution;
	outputResolutionImage.write(fnOut);
	outputResolutionImage() = resolutionChimera;
	outputResolutionImage.write(fnchim);


	#ifdef DEBUG
		outputResolution.write("resolution_simple.vol");
	#endif


	MetaData md;
	size_t objId;
	objId = md.addObject();
	md.setValue(MDL_IMAGE, fnOut, objId);
	md.setValue(MDL_COUNT, (size_t) NVoxelsOriginalMask, objId);
	md.setValue(MDL_COUNT2, (size_t) Nvoxels, objId);

	md.write(fnMd);
}
