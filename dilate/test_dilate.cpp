#include <chrono>
#include <opencv2/core/core.hpp>
#include <opencv2/ocl/ocl.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "utility.hpp"
#include "dilate.pencil.h"

void time_dilate( const std::vector<carp::record_t>& pool, const std::vector<int>& elemsizes, int iteration )
{
    carp::Timing timing("dilate image");

    for ( int q=0; q<iteration; q++ ) {
        for ( auto & item : pool ) {
            for ( auto & elemsize : elemsizes ) {
                // acquiring the image for the test
                cv::Mat cpu_gray;
                cv::cvtColor( item.cpuimg(), cpu_gray, CV_RGB2GRAY );

                cv::Point anchor( elemsize/2, elemsize/2 );
                cv::Size ksize(elemsize, elemsize);
                cv::Mat structuring_element = cv::getStructuringElement( cv::MORPH_ELLIPSE, ksize, anchor );

                cv::Mat cpu_result, gpu_result, pen_result;
                std::chrono::duration<double> elapsed_time_cpu, elapsed_time_gpu_p_copy, elapsed_time_gpu_nocopy, elapsed_time_pencil;

                {
                    const auto cpu_start = std::chrono::high_resolution_clock::now();
                    cv::dilate( cpu_gray, cpu_result, structuring_element, anchor, 1, cv::BORDER_CONSTANT );
                    const auto cpu_end = std::chrono::high_resolution_clock::now();
                    elapsed_time_cpu = cpu_end - cpu_start;
                }
                {
                    const auto gpu_start_copy = std::chrono::high_resolution_clock::now();
                    cv::ocl::oclMat gpu_gray(cpu_gray);
                    cv::ocl::oclMat result;
                    const auto gpu_start = std::chrono::high_resolution_clock::now();
                    cv::ocl::dilate( gpu_gray, result, structuring_element, anchor, 1, cv::BORDER_CONSTANT );
                    const auto gpu_end = std::chrono::high_resolution_clock::now();
                    gpu_result = result;
                    const auto gpu_end_copy = std::chrono::high_resolution_clock::now();
                    elapsed_time_gpu_p_copy = gpu_end_copy - gpu_start_copy;
                    elapsed_time_gpu_nocopy = gpu_end      - gpu_start;
                }
                {
                    pen_result = cv::Mat(cpu_gray.size(), CV_8U);

                    const auto pencil_start = std::chrono::high_resolution_clock::now();
                    pencil_dilate( cpu_gray.rows, cpu_gray.cols, cpu_gray.step1(), cpu_gray.ptr()
                                 , pen_result.step1(), pen_result.ptr()
                                 , structuring_element.rows, structuring_element.cols, structuring_element.step1(), structuring_element.ptr()
                                 , anchor.x, anchor.y
                                 );
                    const auto pencil_end = std::chrono::high_resolution_clock::now();
                    elapsed_time_pencil = pencil_end - pencil_start;
                }
                // Verifying the results
                if ( (cv::norm(cpu_result - gpu_result) > 0.01) || (cv::norm(cpu_result - pen_result) > 0.01) ) {
                    cv::imwrite("cpu_dilate.png", cpu_result);
                    cv::imwrite("gpu_dilate.png", gpu_result );
                    cv::imwrite("pencil_dilate.png", pen_result );
                    throw std::runtime_error("The GPU results are not equivalent with the CPU results.");
                }
                timing.print( elapsed_time_cpu, elapsed_time_gpu_p_copy, elapsed_time_gpu_nocopy, elapsed_time_pencil );
            }
        }
    }
}

int main(int argc, char* argv[])
{

    std::cout << "This executable is iterating over all the files which are present in the directory `./pool'. " << std::endl;

    auto pool = carp::get_pool("pool");

#ifdef RUN_ONLY_ONE_EXPERIMENT
    time_dilate( pool, { 7 }, 1 );
#else
    time_dilate( pool, { 3, 5, 7, 9 }, 6 );
#endif

    return EXIT_SUCCESS;
}
