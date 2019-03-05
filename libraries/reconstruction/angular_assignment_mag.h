/***************************************************************************
 *
 * Authors:     Carlos Oscar S. Sorzano (coss@cnb.csic.es)
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

#ifndef __ANGULAR_ASSIGNMENT_MAG_H
#define __ANGULAR_ASSIGNMENT_MAG_H

#include <../../xmippCore/core/xmipp_program.h>
#include <../../xmippCore/core/xmipp_fftw.h>
#include <../../xmippCore/core/metadata_extension.h>
#include <../../xmippCore/core/multidim_array.h>
#include <../../xmipp/libraries/data/mask.h>

#include <vector>
#include <fstream> // borrar luego
#include <ctime>


/**@defgroup AngularAssignmentMag ***
   @ingroup ReconsLibrary */
//@{

/** Angular Assignment mag parameters. */
class ProgAngularAssignmentMag: public XmippMetadataProgram
{
public:
    /** Filenames */
    FileName fnIn, fnOut, fnDir, fnSym, fnRef;
public: // Internal members
    // Metadata with input images and input volumes
    MetaData mdIn, mdRef, mdOut;

    // vector of reference images
    std::vector< MultidimArray<double> > vecMDaRef;

    // vector of Fourier of reference images
    std::vector< MultidimArray< std::complex<double> > > vecMDaRefF;

    // vector of Fourier of polar representation of magnitude spectrum of reference images
    std::vector< MultidimArray< std::complex<double> > > vecMDaRefFMs_polarF;

    // Size of the images
    size_t Xdim, Ydim;

    // Transformer
    FourierTransformer transformerImage, transformerPolarImage;


    // "delay axes"
    MultidimArray<double> axRot;
    MultidimArray<double> axTx;
    MultidimArray<double> axTy;

    // CCV result matrix
    MultidimArray<double>                   ccMatrixRot;
    MultidimArray<double>                   ccVectorRot;
    std::vector<double>                     cand; // rotation candidates
    int                                     peaksFound = 0; // peaksFound in ccVectorRot
    double                                  tempCoeff;

    size_t idxOut; // index for metadata output file

    int testCounter = 0;

    // candidates for each loop
    std::vector<unsigned int>               candidatesFirstLoop;
    std::vector<unsigned int>               Idx;
    std::vector<double>                     candidatesFirstLoopCoeff;
    std::vector<double>                     bestTx;
    std::vector<double>                     bestTy;
    std::vector<double>                     bestRot;

    // some constants
    int sizeMdRef;
    // some constants
    size_t n_bands;
    size_t startBand;
    size_t finalBand;
    size_t n_rad;
    size_t n_ang;
    size_t n_ang2;
    double maxShift;


public:
    // constructor
    ProgAngularAssignmentMag();

    // destructor
    ~ProgAngularAssignmentMag();

    /// Read arguments from command line
    void defineParams();
    void readParams();

    /** Show. */
    void show();

    /** Run. */
//    void run();

    /*startProcessing() por qué es importante?*/
    void startProcessing();

    /*  */

    /// Produce side info: fill arrays with relevant transformation matrices
    void preProcess();

    /*void processImage()
    *
    */
    void processImage(const FileName &fnImg, const FileName &fnImgOut, const MDRow &rowIn, MDRow &rowOut);

    /* void postProcess(), quizá sea para escribir los datos de salida*/
    void postProcess();

private:
    void printSomeValues(MultidimArray<double> & MDa);
    void pearsonCorr(MultidimArray<double> &X, MultidimArray<double> &Y, double &coeff);
    void arithmetic_mean_and_stddev(MultidimArray<double> &data, double &avg, double &stddev);
    double mean_of_products(MultidimArray<double> &data1, MultidimArray<double> &data2);
    void _writeTestFile(MultidimArray<double> &data, const char *fileName);
    void _writeTestFile(MultidimArray<double> &data, const char *fileName, size_t nFil, size_t nCol);
    void _applyFourierImage(MultidimArray<double> &data, MultidimArray<std::complex<double> > &FourierData);
    void _applyFourierImage(MultidimArray<double> &data, MultidimArray<std::complex<double> > &FourierData, const size_t &ang);
    void _getComplexMagnitude(MultidimArray<std::complex<double> > &FourierData, MultidimArray<double> &FourierMag);
    MultidimArray<double> imToPolar(MultidimArray<double> &cartIm);
    double interpolate(MultidimArray<double> &cartIm, double &x_coord, double &y_coord);
    void completeFourierShift(MultidimArray<double> &in, MultidimArray<double> &out);
    void ccMatrix(MultidimArray<std::complex<double> > &F1, MultidimArray<std::complex<double> > &F2, MultidimArray<double> &result);
    void selectBands(MultidimArray<double> &in, MultidimArray<double> &out);
    void maxByColumn(MultidimArray<double> &in, MultidimArray<double> &out);
    void rotCandidates(MultidimArray<double> &in, std::vector<double>& cand, const size_t &size, int *nPeaksFound);
    void _delayAxes(const size_t &Ydim, const size_t &Xdim, const size_t &n_ang);
    void bestCand(MultidimArray<double> &MDaIn, MultidimArray<std::complex<double> > &MDaInF, MultidimArray<double> &MDaRef, std::vector<double> &cand, int &peaksFound, double *bestCandRot, double *shift_x, double *shift_y, double *bestCoeff);
    void _applyRotation(MultidimArray<double> &MDaRef, double &rot, MultidimArray<double> &MDaRefRot);
    void maxByRow(MultidimArray<double> &in, MultidimArray<double> &out);
    void getShift(MultidimArray<double> &axis, MultidimArray<double> &ccVector, double &shift, const size_t &size);
    void _applyShift(MultidimArray<double> &MDaRef, double &tx, double &ty, MultidimArray<double> &MDaRefShift);
    void ssimIndex(MultidimArray<double> &X, MultidimArray<double> &Y, double &coeff);
    void bestCand2(MultidimArray<double> &MDaIn, MultidimArray<std::complex<double> > &MDaInF, MultidimArray<double> &MDaRef, std::vector<double> &cand, int &peaksFound, double *bestCandRot, double *shift_x, double *shift_y, double *bestCoeff);
    void _applyRotationAndShift(MultidimArray<double> &MDaRef, double &rot, double &tx, double &ty, MultidimArray<double> &MDaRefRot);
    void rotCandidates2(MultidimArray<double> &in, std::vector<double> &cand, const size_t &size, int *nPeaksFound);
    void _applyFourierImage2(MultidimArray<double> &data, MultidimArray<std::complex<double> > &FourierData);
    void _applyFourierImage2(MultidimArray<double> &data, MultidimArray<std::complex<double> > &FourierData, const size_t &ang);
    void halfFourierShift(MultidimArray<double> &in, MultidimArray<double> &out);
    MultidimArray<double> imToPolar2(MultidimArray<double> &cartIm, const size_t &rad, const size_t &ang);
    void rotCandidates3(MultidimArray<double> &in, std::vector<double> &cand, const size_t &size, int *nPeaksFound);
    void fast_correlation_vector2(MultidimArray<std::complex<double> > FFT1, const MultidimArray<std::complex<double> > FFT2, MultidimArray< double >& R, FourierTransformer &transformer);

    void _applyCircularMask(const MultidimArray<double> &in, MultidimArray<double> &out);
    void newApplyGeometry(MultidimArray<double> &in, MultidimArray<double> &out, const double &a, const double &b, const double &c, const double &d, const double &tx, const double &ty);
};
//@}


#endif
