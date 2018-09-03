#include <iostream>
#include <audio_processing.h>
#include <tempogram_processing.h>
#include <tempogram_utils.h>
#include <curve_utils.h>
#include <generic_algothms.h>
#include <defines.h>
#include <signal_utils.h>
#include <args.hxx>

using namespace std::chrono;
using namespace tempogram;

int main(int argc, char **argv) {
    args::ArgumentParser parser(
            "CLI for tempo estimation developed by Egor Dmitriev.\nVisit github for library version.");
    args::HelpFlag help(parser, "help", "Display the help menu", {'h', "help"});
    args::ValueFlag<int> tempo_window_arg(parser, "tempo_window", "Analysis window length in seconds for calculating tempogram", {"tempo_window"}, 8);
    args::ValueFlag<int> ref_tempo_arg(parser, "ref_tempo", "Reference tempo defining the partition of BPM into tempo octaves for calculating cyclic tempogram", {"ref_tempo"}, 60);
    args::ValueFlag<int> octave_divider_arg(parser, "octave_divider", "Number of tempo classes used for representing a tempo octave. This parameter controls the dimensionality of cyclic tempogram", {"octave_divider"}, 120);
    args::ValueFlag<int> smooth_length_arg(parser, "smooth_length", "Length over which the tempogram will be stabilized to extract a steady tempo", {"smooth_length"}, 100);
    args::ValueFlag<float> triplet_weigh_arg(parser, "triplet_weight", "Weight of the triplet intensity which will be adeed to its base intensity", {"triplet_weight"}, 0.8f);
    args::ValueFlag<int> min_section_length_arg(parser, "min_section_length", "Minimum length for a tempo section in samples", {"min_section_length"}, 40);
    args::ValueFlag<int> max_section_length_arg(parser, "max_section_length", "Maximum section length in seconds after which section is split in half", {"max_section_length"}, 60);
    args::ValueFlag<float> bpm_doubt_window_arg(parser, "bpm_doubt_window", "Window around candidate bpm which to search for a more fine and correct bpm", {"bpm_doubt_window"}, 2.f);
    args::ValueFlag<float> bpm_doubt_step_arg(parser, "bpm_doubt_step", "Steps which to take inside the doubt window to fine tune the bpm", {"bpm_doubt_step"}, 0.1f);
    args::ValueFlagList<int> tempo_multiples_arg(parser, "tempo_multiples", "Tempo multiples to consider when searching for correct offset", {"tempo_multiples", 'm'}, {1,2,4});
    args::ValueFlag<bool> generate_click_track_arg(parser, "generate_click_track", "Wether or not a click track should be generated", {"generate_click_track", 'c'}, true);
    args::ValueFlag<int> click_track_subdivision_arg(parser, "click_track_subdivision", "Click subdivision for the click track", {"click_track_subdivision"}, 8);
    args::Flag osu_arg(parser, "osu", "Wether or not to generate tempo data in osu format.", {"osu"});
    args::Positional<std::string> foo(parser, "audio", "Audio file to extract tempo of.");

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (args::Help&)
    {
        std::cout << parser;
        return 0;
    }
    catch (args::ParseError &e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }
    catch (args::ValidationError &e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }


}
/*

int main(int argc, char **argv) {
    if (argc <= 1) {
        std::cerr << "Please specify an audio file" << std::endl;
        exit(1);
    }

    std::string audio_path(argv[1]);
    auto audio = tempogram::audio::open_audio(audio_path.c_str());


    mat reduced_sig = mean(audio.data, 0);
    vec signal = reduced_sig.row(0).t();

    auto nov_cv = tempogram_processing::audio_to_novelty_curve(signal, audio.sr);

    vec bpm = regspace(30, 600);
    auto tempogram_tpl = tempogram_processing::novelty_curve_to_tempogram_dft(std::get<0>(nov_cv), bpm,
                                                                              std::get<1>(nov_cv), 8);

    auto normalized_tempogram = tempogram::normalize_feature(std::get<0>(tempogram_tpl), 2, 0.0001);
    auto cyclic_tempgram = tempogram_processing::tempogram_to_cyclic_tempogram(normalized_tempogram,
                                                                               std::get<1>(tempogram_tpl), 120);

    auto t = std::get<2>(tempogram_tpl);
    t = t(span(100, t.n_rows - 1));
    auto smooth_tempogram = tempogram_utils::smoothen_tempogram(std::get<0>(cyclic_tempgram),
                                                                std::get<1>(cyclic_tempgram), 100);

    auto tempo_curve = tempogram_utils::extract_tempo_curve(smooth_tempogram, std::get<1>(cyclic_tempgram));
    tempo_curve = curve_utils::correct_curve_by_length(tempo_curve, 40);

    auto tempo_segments = curve_utils::split_curve(tempo_curve);
    auto tempo_sections_tmp = curve_utils::tempo_segments_to_sections(tempo_segments, tempo_curve, t,
                                                                      DEFAULT_REF_TEMPO);
    std::vector<curve_utils::Section> tempo_sections;
    for (const auto &section : tempo_sections_tmp) curve_utils::split_section(section, tempo_sections, 60);

    for (auto &section : tempo_sections) {
        section.bpm *= 2;
        curve_utils::extract_offset(std::get<0>(nov_cv), section, {1, 2, 4}, std::get<1>(nov_cv));
        curve_utils::correct_offset(section, 4);
    }

    for (auto &section : tempo_sections) {
        std::cout << section << std::endl;
    }

    vec click_track(audio.data.n_cols);
    for (auto &section : tempo_sections) {
        auto len = (unsigned long) ((section.end - section.start) * audio.sr);
        vec clicks = signal_utils::generate_click_track(section.bpm, section.offset - section.start, 8, len, audio.sr);

        auto start = (unsigned long) (section.start * audio.sr);
        click_track(span(start, start + clicks.n_rows - 1)) = clicks;
    }

    for (int c = 0; c < audio.data.n_rows; ++c) {
        audio.data(c, span::all) = clamp(audio.data(c, span::all) + click_track.t(), -1, 1);
    }

    // Save audio file
    std::string output_path = audio_path;
    size_t ext_pos = audio_path.find_last_of('.');
    if (ext_pos != std::string::npos) {
        output_path = output_path.substr(0, ext_pos);
    }

    output_path += "_processed";

    if (audio.format & SF_FORMAT_FLAC) {
        output_path += ".flac";
    } else if (audio.format & SF_FORMAT_WAV) {
        output_path += ".wav";
    } else {
        output_path += ".idk";
    }

    audio.save(output_path.c_str());

    return 0;
}*/
