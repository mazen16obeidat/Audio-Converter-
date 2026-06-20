#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <cstdint>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_ask.H>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include <lame/lame.h>
#include <FLAC/stream_encoder.h>

struct AudioData {
    unsigned int sampleRate = 0;
    unsigned int channels   = 0;
    uint64_t     totalFrames= 0;
    std::vector<int16_t> samples;
};

std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("Cannot open: " + path);
    auto sz = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(sz));
    in.read(reinterpret_cast<char*>(data.data()), sz);
    return data;
}

AudioData decodeMP3(const std::string& path) {
    auto data = readFile(path);
    mp3dec_t mp3d;
    mp3dec_init(&mp3d);
    AudioData out;
    mp3dec_frame_info_t info;
    std::vector<int16_t> buf(MINIMP3_MAX_SAMPLES_PER_FRAME * 2);
    size_t off = 0;
    bool first = true;
    while (off < data.size()) {
        int n = mp3dec_decode_frame(&mp3d, data.data() + off,
                                   static_cast<int>(data.size() - off),
                                   buf.data(), &info);
        if (n < 0) break;
        off += info.frame_bytes;
        if (first) { out.sampleRate = info.hz; out.channels = info.channels; first = false; }
        out.samples.insert(out.samples.end(), buf.begin(), buf.begin() + n * info.channels);
    }
    out.totalFrames = out.samples.size() / out.channels;
    return out;
}

AudioData decodeFLAC(const std::string& path) {
    auto data = readFile(path);
    drflac* p = drflac_open_memory(data.data(), data.size(), nullptr);
    if (!p) throw std::runtime_error("FLAC decode fail");
    AudioData out;
    out.sampleRate = p->sampleRate;
    out.channels   = p->channels;
    out.totalFrames= p->totalPCMFrameCount;
    out.samples.resize(out.totalFrames * out.channels);
    drflac_uint64 r = drflac_read_pcm_frames_s16(p, out.totalFrames, out.samples.data());
    drflac_close(p);
    if (r != out.totalFrames) throw std::runtime_error("FLAC read incomplete");
    return out;
}

AudioData decodeWAV(const std::string& path) {
    auto data = readFile(path);
    drwav w;
    if (!drwav_init_memory(&w, data.data(), data.size(), nullptr))
        throw std::runtime_error("WAV decode fail");
    AudioData out;
    out.sampleRate = w.sampleRate;
    out.channels   = w.channels;
    out.totalFrames= w.totalPCMFrameCount;
    out.samples.resize(out.totalFrames * out.channels);
    drwav_uint64 r = drwav_read_pcm_frames_s16(&w, out.totalFrames, out.samples.data());
    drwav_uninit(&w);
    if (r != out.totalFrames) throw std::runtime_error("WAV read incomplete");
    return out;
}

void encodeWAV(const AudioData& a, const std::string& path) {
    drwav w;
    drwav_data_format fmt;
    fmt.container = drwav_container_riff;
    fmt.format    = DR_WAVE_FORMAT_PCM;
    fmt.channels  = a.channels;
    fmt.sampleRate= a.sampleRate;
    fmt.bitsPerSample = 16;
    if (!drwav_init_file_write(&w, path.c_str(), &fmt, nullptr))
        throw std::runtime_error("Cannot create WAV: " + path);
    drwav_uint64 wr = drwav_write_pcm_frames(&w, a.totalFrames, a.samples.data());
    drwav_uninit(&w);
    if (wr != a.totalFrames) throw std::runtime_error("WAV write incomplete");
}

void encodeFLAC(const AudioData& a, const std::string& path) {
    FLAC__StreamEncoder* enc = FLAC__stream_encoder_new();
    if (!enc) throw std::runtime_error("FLAC encoder create fail");
    FLAC__stream_encoder_set_verify(enc, false);
    FLAC__stream_encoder_set_compression_level(enc, 5);
    FLAC__stream_encoder_set_channels(enc, a.channels);
    FLAC__stream_encoder_set_bits_per_sample(enc, 16);
    FLAC__stream_encoder_set_sample_rate(enc, a.sampleRate);
    FLAC__stream_encoder_set_total_samples_estimate(enc, a.totalFrames);

    if (FLAC__stream_encoder_init_file(enc, path.c_str(), nullptr, nullptr)
        != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        FLAC__stream_encoder_delete(enc);
        throw std::runtime_error("FLAC encoder init fail");
    }
    std::vector<int32_t> buf(a.samples.size());
    for (size_t i = 0; i < a.samples.size(); ++i)
        buf[i] = static_cast<int32_t>(a.samples[i]);
    FLAC__bool ok = FLAC__stream_encoder_process_interleaved(
        enc, buf.data(), static_cast<uint32_t>(a.totalFrames));
    FLAC__stream_encoder_finish(enc);
    FLAC__stream_encoder_delete(enc);
    if (!ok) throw std::runtime_error("FLAC encode fail");
}

void encodeMP3(AudioData& a, const std::string& path) {
    lame_global_flags* g = lame_init();
    if (!g) throw std::runtime_error("lame_init fail");
    lame_set_in_samplerate(g, a.sampleRate);
    lame_set_num_channels(g, a.channels);
    lame_set_out_samplerate(g, a.sampleRate);
    lame_set_VBR(g, vbr_mtrh);
    lame_set_VBR_quality(g, 2);
    if (lame_init_params(g) < 0) { lame_close(g); throw std::runtime_error("lame params fail"); }
    const int sz = static_cast<int>(1.25 * a.totalFrames) + 7200;
    std::vector<unsigned char> buf(sz);
    int wr = lame_encode_buffer_interleaved(g, a.samples.data(),
                                           static_cast<int>(a.totalFrames),
                                           buf.data(), sz);
    if (wr < 0) { lame_close(g); throw std::runtime_error("MP3 encode error"); }
    int fl = lame_encode_flush(g, buf.data() + wr, sz - wr);
    if (fl < 0) { lame_close(g); throw std::runtime_error("MP3 flush error"); }
    wr += fl;
    std::ofstream out(path, std::ios::binary);
    if (!out) { lame_close(g); throw std::runtime_error("Cannot write MP3: " + path); }
    out.write(reinterpret_cast<const char*>(buf.data()), wr);
    lame_close(g);
}

enum class Format { WAV, FLAC, MP3, Unknown };

Format extensionToFormat(const std::string& path) {
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return Format::Unknown;
    std::string ext = path.substr(dot+1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == "wav")  return Format::WAV;
    if (ext == "flac") return Format::FLAC;
    if (ext == "mp3")  return Format::MP3;
    return Format::Unknown;
}

std::string replaceExtension(const std::string& path, const std::string& newExt) {
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return path + "." + newExt;
    return path.substr(0, dot) + "." + newExt;
}

// ---------- Smart default output extension ----------
std::string defaultOutputExt(Format inFmt) {
    switch (inFmt) {
        case Format::MP3:  return "flac";
        case Format::FLAC: return "wav";
        case Format::WAV:  return "mp3";
        default:           return "wav";
    }
}

// ---------- Get directory from full path ----------
std::string dirOf(const std::string& path) {
    auto s = path.find_last_of("/\\");
    if (s == std::string::npos) return "";
    return path.substr(0, s);
}

// ---------- Global GUI pointers ----------
Fl_Input*   inPath  = nullptr;
Fl_Input*   outPath = nullptr;
Fl_Box*     status  = nullptr;
Fl_Progress* progress = nullptr;

// remember last folder
std::string lastInputDir, lastOutputDir;

void browseInput(Fl_Widget*, void*) {
    Fl_Native_File_Chooser chooser;
    chooser.title("Select input audio file");
    chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
    chooser.filter("Audio\t*.wav;*.flac;*.mp3");
    if (!lastInputDir.empty()) chooser.directory(lastInputDir.c_str());
    if (chooser.show() == 0) {
        inPath->value(chooser.filename());
        lastInputDir = dirOf(chooser.filename());
    }
}

void browseOutput(Fl_Widget*, void*) {
    Fl_Native_File_Chooser chooser;
    chooser.title("Save output file as");
    chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    chooser.filter("Audio\t*.wav;*.flac;*.mp3");
    if (!lastOutputDir.empty()) chooser.directory(lastOutputDir.c_str());
    if (chooser.show() == 0) {
        outPath->value(chooser.filename());
        lastOutputDir = dirOf(chooser.filename());
    }
}

void handleDroppedFile(const char* text) {
    if (!text || strlen(text) == 0) return;
    std::string first(text);
    auto nl = first.find('\n');
    if (nl != std::string::npos) first = first.substr(0, nl);
    inPath->value(first.c_str());

    if (strlen(outPath->value()) == 0) {
        Format fmt = extensionToFormat(first);
        std::string ext = defaultOutputExt(fmt);
        std::string out = replaceExtension(first, ext);
        outPath->value(out.c_str());
        status->label(("Output set to " + ext + ". Drop again or browse to change.").c_str());
    } else {
        status->label("Input set from drag-and-drop.");
    }
}

void clearFields(Fl_Widget*, void*) {
    inPath->value("");
    outPath->value("");
    status->label("Cleared. Drop a file or browse.");
    progress->value(0);
}

void swapFields(Fl_Widget*, void*) {
    std::string inp = inPath->value();
    std::string out = outPath->value();
    inPath->value(out.c_str());
    outPath->value(inp.c_str());
    status->label("Input/output swapped.");
}

void convert(Fl_Widget*, void*) {
    try {
        std::string inp = inPath->value();
        std::string out = outPath->value();
        if (inp.empty() || out.empty()) {
            status->label("Please provide input and output files.");
            return;
        }

        // Overwrite check
        std::ifstream existCheck(out);
        if (existCheck) {
            int choice = fl_choice("Output file already exists.\nOverwrite?",
                                   "No", "Yes", nullptr);
            if (choice != 1) {
                status->label("Conversion cancelled.");
                return;
            }
        }

        Format inFmt = extensionToFormat(inp);
        Format outFmt = extensionToFormat(out);
        if (inFmt == Format::Unknown || outFmt == Format::Unknown) {
            status->label("Unsupported format.");
            return;
        }
        if (inFmt == outFmt) {
            status->label("Input and output formats are the same.");
            return;
        }

        progress->value(0);
        status->label("Converting...");
        Fl::check();

        AudioData audio;
        switch (inFmt) {
            case Format::MP3:  audio = decodeMP3(inp);  break;
            case Format::FLAC: audio = decodeFLAC(inp); break;
            case Format::WAV:  audio = decodeWAV(inp);  break;
            default: break;
        }

        switch (outFmt) {
            case Format::MP3:  encodeMP3(audio, out);  break;
            case Format::FLAC: encodeFLAC(audio, out); break;
            case Format::WAV:  encodeWAV(audio, out);  break;
            default: break;
        }

        progress->value(100);
        status->label("Conversion successful!");
    } catch (const std::exception& e) {
        status->copy_label(e.what());
        progress->value(0);
    }
}

class DropWindow : public Fl_Window {
public:
    DropWindow(int w, int h, const char* t) : Fl_Window(w, h, t) {}
protected:
    int handle(int e) override {
        switch (e) {
            case FL_DND_ENTER:
            case FL_DND_DRAG: return 1;
            case FL_DND_RELEASE:
                handleDroppedFile(Fl::event_text());
                return 1;
        }
        return Fl_Window::handle(e);
    }
};

int main(int argc, char** argv) {
    DropWindow* win = new DropWindow(520, 220, "Audio Converter");

    inPath  = new Fl_Input(100, 20, 280, 25, "Input:");
    Fl_Button* inBr = new Fl_Button(390, 20, 80, 25, "Browse...");
    inBr->callback(browseInput);

    outPath = new Fl_Input(100, 50, 280, 25, "Output:");
    Fl_Button* outBr = new Fl_Button(390, 50, 80, 25, "Browse...");
    outBr->callback(browseOutput);

    Fl_Button* swapBtn = new Fl_Button(240, 85, 70, 22, "Swap");
    swapBtn->callback(swapFields);

    Fl_Button* clearBtn = new Fl_Button(320, 85, 70, 22, "Clear");
    clearBtn->callback(clearFields);

    Fl_Button* convBtn = new Fl_Button(200, 115, 100, 30, "Convert");
    convBtn->callback(convert);

    progress = new Fl_Progress(50, 155, 400, 20);
    progress->minimum(0);
    progress->maximum(100);
    progress->value(0);

    status = new Fl_Box(10, 185, 500, 25, "Drop a file here or use Browse.");
    status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    win->end();
    win->show();

    // Command‑line argument → pre‑fill input + auto‑generate output
    if (argc == 2) {
        inPath->value(argv[1]);
        Format fmt = extensionToFormat(argv[1]);
        std::string ext = defaultOutputExt(fmt);
        outPath->value(replaceExtension(argv[1], ext).c_str());
        status->label("File loaded from command line.");
    }

    return Fl::run();
}