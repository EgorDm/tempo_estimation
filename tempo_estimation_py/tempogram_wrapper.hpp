//
// Created by egordm on 25-7-2018.
//

#ifndef TEMPOGRAM_TEMPO_ESTIMATION_TEMPOGRAM_WRAPPER_H
#define TEMPOGRAM_TEMPO_ESTIMATION_TEMPOGRAM_WRAPPER_H

#include "pyarma.hpp"
#include "tempogram.hpp"

namespace tempogram_wrapper {
    inline py::array novelty_curve_to_tempogram_dft(pyarr_d novelty_curve_np, pyarr_d bpm_np, double feature_rate,
                                                 int tempo_window, int hop_length) {
        arma::vec novelty_curve = py_to_vec(novelty_curve_np);
        arma::vec bpm = py_to_vec(bpm_np);
        auto ret = tempogram::novelty_curve_to_tempogram_dft(novelty_curve, bpm, feature_rate, tempo_window, hop_length);
        return mat_to_py(ret);
    }
}

#endif //TEMPOGRAM_TEMPO_ESTIMATION_TEMPOGRAM_WRAPPER_H