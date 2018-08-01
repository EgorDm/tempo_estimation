//
// Created by egordm on 28-7-2018.
//

#include <sigpack.h>
#include "tempo_estimation.h"
#include "math_utils.hpp"
#include "resample.h"

using namespace sp;

std::tuple<cx_mat, vec, vec> tempogram::novelty_curve_to_tempogram_dft(vec &novelty_curve, vec &bpm, double feature_rate,
                                                                       int tempo_window)  {
    return novelty_curve_to_tempogram_dft(novelty_curve, bpm, feature_rate, tempo_window,
                                          (int) ceil(feature_rate / 5.));
}

std::tuple<cx_mat, vec, vec>
tempogram::novelty_curve_to_tempogram_dft(vec &novelty_curve, vec &bpm, double feature_rate, int tempo_window,
                                          int hop_length)  {
    auto win_length = static_cast<int>(round(tempo_window * feature_rate));
    win_length = win_length + (win_length % 2) - 1;

    auto window = sp::hann(static_cast<const uword>(win_length));
    auto half_window = (int) round(win_length / 2.0);

    vec pad(static_cast<const uword>(half_window), fill::zeros);

    novelty_curve = join_cols(pad, novelty_curve);
    novelty_curve = join_cols(novelty_curve, pad);

    auto ret = tempogram::compute_fourier_coefficients(novelty_curve, window, win_length - hop_length, bpm / 60.,
                                            feature_rate);

    std::get<0>(ret) = std::get<0>(ret) / sqrt((double) win_length) / sum(window) * win_length;
    std::get<1>(ret) = std::get<1>(ret) * 60;
    std::get<2>(ret) = std::get<2>(ret) - std::get<2>(ret)(0);

    return ret;
}


std::tuple<vec, int>
tempogram::audio_to_novelty_curve(const vec &signal, int sr, int window_length, int hop_length, double compression_c,
                                  bool log_compression, int resample_feature_rate) {
    if (window_length <= 0) window_length = static_cast<int>(1024 * sr / 22050.);
    if (hop_length <= 0) hop_length = static_cast<int>(512 * sr / 22050.);

    vec window = utils::math::my_hanning(static_cast<const uword>(window_length));
    auto stft_ret = stft(signal, sr, window, window_length, hop_length);
    float feature_rate = std::get<1>(stft_ret);


    // normalize and convert to dB
    mat spe = std::get<0>(stft_ret) / std::get<0>(stft_ret).max();
    double thresh = -74;
    thresh = pow(10, thresh / 20);
    spe = clamp(spe, thresh, spe.max());

    // bandwise processing
    mat bands = {{0,      500},
                 {500,    1250},
                 {1250,   3125},
                 {3125,   7812.5},
                 {7812.5, floor(sr / 2.)}};
    mat band_novelty_curves(bands.n_rows, spe.n_cols, fill::zeros);

    for (uword i = 0; i < bands.n_rows; ++i) {
        rowvec bins = round(bands(i, span::all) / (sr / (double) window_length));
        bins = clamp(bins, 0, window_length / 2.);

        // band novelty curve
        mat band_data = spe(span((const uword) bins(0), (const uword) bins(1) - 1), span::all);
        if (log_compression && compression_c > 0)
            band_data = log(1 + band_data * compression_c) / log(1 + compression_c);

        // smoothed differentiator
        double diff_len = 0.3;
        diff_len = max(ceil(diff_len * sr / (double) hop_length), 5.);
        diff_len = 2 * round(diff_len / 2.) + 1;
        auto half_diff_len = (const uword)floor(diff_len / 2);

        vec mult_filt = join_cols(-1 * vec(half_diff_len, fill::ones), vec(1, fill::zeros));
        mult_filt = join_cols(mult_filt, vec(half_diff_len, fill::ones));
        rowvec diff_filter = (utils::math::my_hanning((const uword) diff_len) % mult_filt).t();

        mat band_krn = utils::math::pad_mat(band_data, (int)half_diff_len);
        mat band_diff = conv2(band_krn, flipud(fliplr(diff_filter)), "same");
        band_diff = band_diff % (band_diff > 0);
        band_diff = band_diff(span::all, span(half_diff_len - 1, band_diff.n_cols - half_diff_len - 2));

        // normalize band
        double norm_len = 5; // sec
        norm_len = max(ceil(norm_len * sr / hop_length), 3.);
        rowvec norm_filter = utils::math::my_hanning((const uword)norm_len).t();
        mat norm_curve = conv2(sum(band_data, 0), flipud(fliplr(norm_filter / sum(norm_filter))), "same");

        // boundary correction
        double norm_filter_sum_sub = sum(sum(norm_filter));
        mat norm_filter_sum = (norm_filter_sum_sub - cumsum(norm_filter, 1)) / norm_filter_sum_sub;
        auto half_norm_len = (const uword)(floor(norm_len / 2.));
        auto f_half_span = span(0, half_norm_len - 1);
        auto l_half_span = span(norm_curve.n_cols - half_norm_len, norm_curve.n_cols - 1);
        norm_curve(0, f_half_span) = norm_curve(0, f_half_span) / fliplr(norm_filter_sum(0, f_half_span));
        norm_curve(0, l_half_span) = norm_curve(0, l_half_span) / norm_filter_sum(0, f_half_span);

        for(uword r = 0; r < band_diff.n_rows; ++r) {
            band_diff(r, span::all) /= norm_curve;
        }

        // compute novelty curve of band
        mat novelty_curve = sum(band_diff, 0);
        band_novelty_curves(i, span::all) = novelty_curve;
    }

    vec novelty_curve = mean(band_novelty_curves, 0).t();

    // resample curve
    if(resample_feature_rate > 0 && resample_feature_rate != feature_rate) {
        const int p = (const int)round(1000 * resample_feature_rate / feature_rate);
        const int q = 1000;

        novelty_curve = tempogram::resample(novelty_curve, p, q);
        feature_rate = (float)resample_feature_rate;
    }

    novelty_curve = novelty_smoothed_subtraction(novelty_curve, sr, hop_length);

    return std::make_tuple(novelty_curve, (int)feature_rate);
}