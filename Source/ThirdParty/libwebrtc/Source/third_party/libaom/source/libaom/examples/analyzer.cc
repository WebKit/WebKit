/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <wx/wx.h>
#include <wx/aboutdlg.h>
#include <wx/cmdline.h>
#include <wx/dcbuffer.h>

#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#include "av1/common/av1_common_int.h"
#include "av1/decoder/accounting.h"
#include "av1/decoder/inspection.h"
#include "common/tools_common.h"
#include "common/video_reader.h"

#define OD_SIGNMASK(a) (-((a) < 0))
#define OD_FLIPSIGNI(a, b) (((a) + OD_SIGNMASK(b)) ^ OD_SIGNMASK(b))
#define OD_DIV_ROUND(x, y) (((x) + OD_FLIPSIGNI((y) >> 1, x)) / (y))

enum {
  OD_LUMA_MASK = 1 << 0,
  OD_CB_MASK = 1 << 1,
  OD_CR_MASK = 1 << 2,
  OD_ALL_MASK = OD_LUMA_MASK | OD_CB_MASK | OD_CR_MASK
};

class AV1Decoder {
 private:
  FILE *input;
  wxString path;

  AvxVideoReader *reader;
  const AvxVideoInfo *info;

  insp_frame_data frame_data;

  aom_codec_ctx_t codec;
  bool show_padding;

 public:
  aom_image_t *image;
  int frame;

  int plane_mask;

  AV1Decoder();
  ~AV1Decoder();

  bool open(const wxString &path);
  void close();
  bool step();

  int getWidthPadding() const;
  int getHeightPadding() const;
  void togglePadding();
  int getWidth() const;
  int getHeight() const;

  bool getAccountingStruct(Accounting **acct);
  bool setInspectionCallback();

  static void inspect(void *decoder, void *data);
};

AV1Decoder::AV1Decoder()
    : reader(NULL), info(NULL), decoder(NULL), show_padding(false), image(NULL),
      frame(0) {}

AV1Decoder::~AV1Decoder() {}

void AV1Decoder::togglePadding() { show_padding = !show_padding; }

bool AV1Decoder::open(const wxString &path) {
  reader = aom_video_reader_open(path.mb_str());
  if (!reader) {
    fprintf(stderr, "Failed to open %s for reading.", path.mb_str().data());
    return false;
  }
  this->path = path;
  info = aom_video_reader_get_info(reader);
  decoder = get_aom_decoder_by_fourcc(info->codec_fourcc);
  if (!decoder) {
    fprintf(stderr, "Unknown input codec.");
    return false;
  }
  printf("Using %s\n", aom_codec_iface_name(decoder));
  if (aom_codec_dec_init(&codec, decoder, NULL, 0)) {
    fprintf(stderr, "Failed to initialize decoder.");
    return false;
  }
  ifd_init(&frame_data, info->frame_width, info->frame_height);
  setInspectionCallback();
  return true;
}

void AV1Decoder::close() {}

bool AV1Decoder::step() {
  if (aom_video_reader_read_frame(reader)) {
    size_t frame_size;
    const unsigned char *frame_data;
    frame_data = aom_video_reader_get_frame(reader, &frame_size);
    if (aom_codec_decode(&codec, frame_data, frame_size, NULL)) {
      fprintf(stderr, "Failed to decode frame.");
      return false;
    } else {
      aom_codec_iter_t iter = NULL;
      image = aom_codec_get_frame(&codec, &iter);
      if (image != NULL) {
        frame++;
        return true;
      }
      return false;
    }
  }
  return false;
}

int AV1Decoder::getWidth() const {
  return info->frame_width + 2 * getWidthPadding();
}

int AV1Decoder::getWidthPadding() const {
  return show_padding ? AOMMAX(info->frame_width + 16,
                               ALIGN_POWER_OF_TWO(info->frame_width, 6)) -
                            info->frame_width
                      : 0;
}

int AV1Decoder::getHeight() const {
  return info->frame_height + 2 * getHeightPadding();
}

int AV1Decoder::getHeightPadding() const {
  return show_padding ? AOMMAX(info->frame_height + 16,
                               ALIGN_POWER_OF_TWO(info->frame_height, 6)) -
                            info->frame_height
                      : 0;
}

bool AV1Decoder::getAccountingStruct(Accounting **accounting) {
  return aom_codec_control(&codec, AV1_GET_ACCOUNTING, accounting) ==
         AOM_CODEC_OK;
}

bool AV1Decoder::setInspectionCallback() {
  aom_inspect_init ii;
  ii.inspect_cb = AV1Decoder::inspect;
  ii.inspect_ctx = (void *)this;
  return aom_codec_control(&codec, AV1_SET_INSPECTION_CALLBACK, &ii) ==
         AOM_CODEC_OK;
}

void AV1Decoder::inspect(void *pbi, void *data) {
  AV1Decoder *decoder = (AV1Decoder *)data;
  ifd_inspect(&decoder->frame_data, pbi, 0);
}

#define MIN_ZOOM (1)
#define MAX_ZOOM (4)

class AnalyzerPanel : public wxPanel {
  DECLARE_EVENT_TABLE()

 private:
  AV1Decoder decoder;
  const wxString path;

  int zoom;
  unsigned char *pixels;

  const bool bit_accounting;
  double *bpp_q3;

  int plane_mask;

  // The display size is the decode size, scaled by the zoom.
  int getDisplayWidth() const;
  int getDisplayHeight() const;

  bool updateDisplaySize();

  void computeBitsPerPixel();

 public:
  AnalyzerPanel(wxWindow *parent, const wxString &path,
                const bool bit_accounting);
  ~AnalyzerPanel();

  bool open(const wxString &path);
  void close();
  void render();
  void togglePadding();
  bool nextFrame();
  void refresh();

  int getZoom() const;
  bool setZoom(int zoom);

  void setShowPlane(bool show_plane, int mask);

  void onPaint(wxPaintEvent &event);  // NOLINT
};

BEGIN_EVENT_TABLE(AnalyzerPanel, wxPanel)
EVT_PAINT(AnalyzerPanel::onPaint)
END_EVENT_TABLE()

AnalyzerPanel::AnalyzerPanel(wxWindow *parent, const wxString &path,
                             const bool bit_accounting)
    : wxPanel(parent), path(path), zoom(0), pixels(NULL),
      bit_accounting(bit_accounting), bpp_q3(NULL), plane_mask(OD_ALL_MASK) {}

AnalyzerPanel::~AnalyzerPanel() { close(); }

void AnalyzerPanel::setShowPlane(bool show_plane, int mask) {
  if (show_plane) {
    plane_mask |= mask;
  } else {
    plane_mask &= ~mask;
  }
}

void AnalyzerPanel::render() {
  aom_image_t *img = decoder.image;
  const int hbd = !!(img->fmt & AOM_IMG_FMT_HIGHBITDEPTH);
  int y_stride = img->stride[0] >> hbd;
  int cb_stride = img->stride[1] >> hbd;
  int cr_stride = img->stride[2] >> hbd;
  int p_stride = 3 * getDisplayWidth();
  unsigned char *y_row = img->planes[0];
  unsigned char *cb_row = img->planes[1];
  unsigned char *cr_row = img->planes[2];
  uint16_t *y_row16 = reinterpret_cast<uint16_t *>(y_row);
  uint16_t *cb_row16 = reinterpret_cast<uint16_t *>(cb_row);
  uint16_t *cr_row16 = reinterpret_cast<uint16_t *>(cr_row);
  unsigned char *p_row = pixels;
  int y_width_padding = decoder.getWidthPadding();
  int cb_width_padding = y_width_padding >> 1;
  int cr_width_padding = y_width_padding >> 1;
  int y_height_padding = decoder.getHeightPadding();
  int cb_height_padding = y_height_padding >> 1;
  int cr_height_padding = y_height_padding >> 1;
  for (int j = 0; j < decoder.getHeight(); j++) {
    unsigned char *y = y_row - y_stride * y_height_padding;
    unsigned char *cb = cb_row - cb_stride * cb_height_padding;
    unsigned char *cr = cr_row - cr_stride * cr_height_padding;
    uint16_t *y16 = y_row16 - y_stride * y_height_padding;
    uint16_t *cb16 = cb_row16 - cb_stride * cb_height_padding;
    uint16_t *cr16 = cr_row16 - cr_stride * cr_height_padding;
    unsigned char *p = p_row;
    for (int i = 0; i < decoder.getWidth(); i++) {
      int64_t yval;
      int64_t cbval;
      int64_t crval;
      int pmask;
      unsigned rval;
      unsigned gval;
      unsigned bval;
      if (hbd) {
        yval = *(y16 - y_width_padding);
        cbval = *(cb16 - cb_width_padding);
        crval = *(cr16 - cr_width_padding);
      } else {
        yval = *(y - y_width_padding);
        cbval = *(cb - cb_width_padding);
        crval = *(cr - cr_width_padding);
      }
      pmask = plane_mask;
      if (pmask & OD_LUMA_MASK) {
        yval -= 16;
      } else {
        yval = 128;
      }
      cbval = ((pmask & OD_CB_MASK) >> 1) * (cbval - 128);
      crval = ((pmask & OD_CR_MASK) >> 2) * (crval - 128);
      /*This is intentionally slow and very accurate.*/
      rval = OD_CLAMPI(
          0,
          (int32_t)OD_DIV_ROUND(
              2916394880000LL * yval + 4490222169144LL * crval, 9745792000LL),
          65535);
      gval = OD_CLAMPI(0,
                       (int32_t)OD_DIV_ROUND(2916394880000LL * yval -
                                                 534117096223LL * cbval -
                                                 1334761232047LL * crval,
                                             9745792000LL),
                       65535);
      bval = OD_CLAMPI(
          0,
          (int32_t)OD_DIV_ROUND(
              2916394880000LL * yval + 5290866304968LL * cbval, 9745792000LL),
          65535);
      unsigned char *px_row = p;
      for (int v = 0; v < zoom; v++) {
        unsigned char *px = px_row;
        for (int u = 0; u < zoom; u++) {
          *(px + 0) = (unsigned char)(rval >> 8);
          *(px + 1) = (unsigned char)(gval >> 8);
          *(px + 2) = (unsigned char)(bval >> 8);
          px += 3;
        }
        px_row += p_stride;
      }
      if (hbd) {
        int dc = ((y16 - y_row16) & 1) | (1 - img->x_chroma_shift);
        y16++;
        cb16 += dc;
        cr16 += dc;
      } else {
        int dc = ((y - y_row) & 1) | (1 - img->x_chroma_shift);
        y++;
        cb += dc;
        cr += dc;
      }
      p += zoom * 3;
    }
    int dc = -((j & 1) | (1 - img->y_chroma_shift));
    if (hbd) {
      y_row16 += y_stride;
      cb_row16 += dc & cb_stride;
      cr_row16 += dc & cr_stride;
    } else {
      y_row += y_stride;
      cb_row += dc & cb_stride;
      cr_row += dc & cr_stride;
    }
    p_row += zoom * p_stride;
  }
}

void AnalyzerPanel::computeBitsPerPixel() {
  Accounting *acct;
  double bpp_total;
  int totals_q3[MAX_SYMBOL_TYPES] = { 0 };
  int sym_count[MAX_SYMBOL_TYPES] = { 0 };
  decoder.getAccountingStruct(&acct);
  for (int j = 0; j < decoder.getHeight(); j++) {
    for (int i = 0; i < decoder.getWidth(); i++) {
      bpp_q3[j * decoder.getWidth() + i] = 0.0;
    }
  }
  bpp_total = 0;
  for (int i = 0; i < acct->syms.num_syms; i++) {
    AccountingSymbol *s;
    s = &acct->syms.syms[i];
    totals_q3[s->id] += s->bits;
    sym_count[s->id] += s->samples;
  }
  printf("=== Frame: %-3i ===\n", decoder.frame - 1);
  for (int i = 0; i < acct->syms.dictionary.num_strs; i++) {
    if (totals_q3[i]) {
      printf("%30s = %10.3f (%f bit/symbol)\n", acct->syms.dictionary.strs[i],
             (float)totals_q3[i] / 8, (float)totals_q3[i] / 8 / sym_count[i]);
    }
  }
  printf("\n");
}

void AnalyzerPanel::togglePadding() {
  decoder.togglePadding();
  updateDisplaySize();
}

bool AnalyzerPanel::nextFrame() {
  if (decoder.step()) {
    refresh();
    return true;
  }
  return false;
}

void AnalyzerPanel::refresh() {
  if (bit_accounting) {
    computeBitsPerPixel();
  }
  render();
}

int AnalyzerPanel::getDisplayWidth() const { return zoom * decoder.getWidth(); }

int AnalyzerPanel::getDisplayHeight() const {
  return zoom * decoder.getHeight();
}

bool AnalyzerPanel::updateDisplaySize() {
  unsigned char *p = (unsigned char *)malloc(
      sizeof(*p) * 3 * getDisplayWidth() * getDisplayHeight());
  if (p == NULL) {
    return false;
  }
  free(pixels);
  pixels = p;
  SetSize(getDisplayWidth(), getDisplayHeight());
  return true;
}

bool AnalyzerPanel::open(const wxString &path) {
  if (!decoder.open(path)) {
    return false;
  }
  if (!setZoom(MIN_ZOOM)) {
    return false;
  }
  if (bit_accounting) {
    bpp_q3 = (double *)malloc(sizeof(*bpp_q3) * decoder.getWidth() *
                              decoder.getHeight());
    if (bpp_q3 == NULL) {
      fprintf(stderr, "Could not allocate memory for bit accounting\n");
      close();
      return false;
    }
  }
  if (!nextFrame()) {
    close();
    return false;
  }
  SetFocus();
  return true;
}

void AnalyzerPanel::close() {
  decoder.close();
  free(pixels);
  pixels = NULL;
  free(bpp_q3);
  bpp_q3 = NULL;
}

int AnalyzerPanel::getZoom() const { return zoom; }

bool AnalyzerPanel::setZoom(int z) {
  if (z <= MAX_ZOOM && z >= MIN_ZOOM && zoom != z) {
    int old_zoom = zoom;
    zoom = z;
    if (!updateDisplaySize()) {
      zoom = old_zoom;
      return false;
    }
    return true;
  }
  return false;
}

void AnalyzerPanel::onPaint(wxPaintEvent &) {
  wxBitmap bmp(wxImage(getDisplayWidth(), getDisplayHeight(), pixels, true));
  wxBufferedPaintDC dc(this, bmp);
}

class AnalyzerFrame : public wxFrame {
  DECLARE_EVENT_TABLE()

 private:
  AnalyzerPanel *panel;
  const bool bit_accounting;

  wxMenu *fileMenu;
  wxMenu *viewMenu;
  wxMenu *playbackMenu;

 public:
  AnalyzerFrame(const bool bit_accounting);  // NOLINT

  void onOpen(wxCommandEvent &event);   // NOLINT
  void onClose(wxCommandEvent &event);  // NOLINT
  void onQuit(wxCommandEvent &event);   // NOLINT

  void onTogglePadding(wxCommandEvent &event);  // NOLINT
  void onZoomIn(wxCommandEvent &event);         // NOLINT
  void onZoomOut(wxCommandEvent &event);        // NOLINT
  void onActualSize(wxCommandEvent &event);     // NOLINT

  void onToggleViewMenuCheckBox(wxCommandEvent &event);          // NOLINT
  void onResetAndToggleViewMenuCheckBox(wxCommandEvent &event);  // NOLINT

  void onNextFrame(wxCommandEvent &event);  // NOLINT
  void onGotoFrame(wxCommandEvent &event);  // NOLINT
  void onRestart(wxCommandEvent &event);    // NOLINT

  void onAbout(wxCommandEvent &event);  // NOLINT

  bool open(const wxString &path);
  bool setZoom(int zoom);
  void updateViewMenu();
};

enum {
  wxID_NEXT_FRAME = 6000,
  wxID_SHOW_Y,
  wxID_SHOW_U,
  wxID_SHOW_V,
  wxID_GOTO_FRAME,
  wxID_RESTART,
  wxID_ACTUAL_SIZE,
  wxID_PADDING
};

BEGIN_EVENT_TABLE(AnalyzerFrame, wxFrame)
EVT_MENU(wxID_OPEN, AnalyzerFrame::onOpen)
EVT_MENU(wxID_CLOSE, AnalyzerFrame::onClose)
EVT_MENU(wxID_EXIT, AnalyzerFrame::onQuit)
EVT_MENU(wxID_PADDING, AnalyzerFrame::onTogglePadding)
EVT_MENU(wxID_ZOOM_IN, AnalyzerFrame::onZoomIn)
EVT_MENU(wxID_ZOOM_OUT, AnalyzerFrame::onZoomOut)
EVT_MENU(wxID_ACTUAL_SIZE, AnalyzerFrame::onActualSize)
EVT_MENU(wxID_SHOW_Y, AnalyzerFrame::onResetAndToggleViewMenuCheckBox)
EVT_MENU(wxID_SHOW_U, AnalyzerFrame::onResetAndToggleViewMenuCheckBox)
EVT_MENU(wxID_SHOW_V, AnalyzerFrame::onResetAndToggleViewMenuCheckBox)
EVT_MENU(wxID_NEXT_FRAME, AnalyzerFrame::onNextFrame)
EVT_MENU(wxID_GOTO_FRAME, AnalyzerFrame::onGotoFrame)
EVT_MENU(wxID_RESTART, AnalyzerFrame::onRestart)
EVT_MENU(wxID_ABOUT, AnalyzerFrame::onAbout)
END_EVENT_TABLE()

AnalyzerFrame::AnalyzerFrame(const bool bit_accounting)
    : wxFrame(NULL, wxID_ANY, _("AV1 Stream Analyzer"), wxDefaultPosition,
              wxDefaultSize, wxDEFAULT_FRAME_STYLE),
      panel(NULL), bit_accounting(bit_accounting) {
  wxMenuBar *mb = new wxMenuBar();

  fileMenu = new wxMenu();
  fileMenu->Append(wxID_OPEN, _("&Open...\tCtrl-O"), _("Open AV1 file"));
  fileMenu->Append(wxID_CLOSE, _("&Close\tCtrl-W"), _("Close AV1 file"));
  fileMenu->Enable(wxID_CLOSE, false);
  fileMenu->Append(wxID_EXIT, _("E&xit\tCtrl-Q"), _("Quit this program"));
  mb->Append(fileMenu, _("&File"));

  wxAcceleratorEntry entries[2];
  entries[0].Set(wxACCEL_CTRL, (int)'=', wxID_ZOOM_IN);
  entries[1].Set(wxACCEL_CTRL | wxACCEL_SHIFT, (int)'-', wxID_ZOOM_OUT);
  wxAcceleratorTable accel(2, entries);
  this->SetAcceleratorTable(accel);

  viewMenu = new wxMenu();
  +viewMenu->Append(wxID_PADDING, _("Toggle padding\tCtrl-p"),
                    _("Show padding"));
  viewMenu->Append(wxID_ZOOM_IN, _("Zoom-In\tCtrl-+"), _("Double image size"));
  viewMenu->Append(wxID_ZOOM_OUT, _("Zoom-Out\tCtrl--"), _("Half image size"));
  viewMenu->Append(wxID_ACTUAL_SIZE, _("Actual size\tCtrl-0"),
                   _("Actual size of the frame"));
  viewMenu->AppendSeparator();
  viewMenu->AppendCheckItem(wxID_SHOW_Y, _("&Y plane\tCtrl-Y"),
                            _("Show Y plane"));
  viewMenu->AppendCheckItem(wxID_SHOW_U, _("&U plane\tCtrl-U"),
                            _("Show U plane"));
  viewMenu->AppendCheckItem(wxID_SHOW_V, _("&V plane\tCtrl-V"),
                            _("Show V plane"));
  mb->Append(viewMenu, _("&View"));

  playbackMenu = new wxMenu();
  playbackMenu->Append(wxID_NEXT_FRAME, _("Next frame\tCtrl-."),
                       _("Go to next frame"));
  /*playbackMenu->Append(wxID_RESTART, _("&Restart\tCtrl-R"),
                       _("Set video to frame 0"));
  playbackMenu->Append(wxID_GOTO_FRAME, _("Jump to Frame\tCtrl-J"),
                       _("Go to frame number"));*/
  mb->Append(playbackMenu, _("&Playback"));

  wxMenu *helpMenu = new wxMenu();
  helpMenu->Append(wxID_ABOUT, _("&About...\tF1"), _("Show about dialog"));
  mb->Append(helpMenu, _("&Help"));

  SetMenuBar(mb);

  CreateStatusBar(1);
}

void AnalyzerFrame::onOpen(wxCommandEvent &WXUNUSED(event)) {
  wxFileDialog openFileDialog(this, _("Open file"), wxEmptyString,
                              wxEmptyString, _("AV1 files (*.ivf)|*.ivf"),
                              wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (openFileDialog.ShowModal() != wxID_CANCEL) {
    open(openFileDialog.GetPath());
  }
}

void AnalyzerFrame::onClose(wxCommandEvent &WXUNUSED(event)) {}

void AnalyzerFrame::onQuit(wxCommandEvent &WXUNUSED(event)) { Close(true); }

void AnalyzerFrame::onTogglePadding(wxCommandEvent &WXUNUSED(event)) {
  panel->togglePadding();
  SetClientSize(panel->GetSize());
  panel->render();
  panel->Refresh();
}

void AnalyzerFrame::onZoomIn(wxCommandEvent &WXUNUSED(event)) {
  setZoom(panel->getZoom() + 1);
}

void AnalyzerFrame::onZoomOut(wxCommandEvent &WXUNUSED(event)) {
  setZoom(panel->getZoom() - 1);
}

void AnalyzerFrame::onActualSize(wxCommandEvent &WXUNUSED(event)) {
  setZoom(MIN_ZOOM);
}

void AnalyzerFrame::onToggleViewMenuCheckBox(wxCommandEvent &event) {  // NOLINT
  GetMenuBar()->Check(event.GetId(), event.IsChecked());
  updateViewMenu();
}

void AnalyzerFrame::onResetAndToggleViewMenuCheckBox(
    wxCommandEvent &event) {  // NOLINT
  int id = event.GetId();
  if (id != wxID_SHOW_Y && id != wxID_SHOW_U && id != wxID_SHOW_V) {
    GetMenuBar()->Check(wxID_SHOW_Y, true);
    GetMenuBar()->Check(wxID_SHOW_U, true);
    GetMenuBar()->Check(wxID_SHOW_V, true);
  }
  onToggleViewMenuCheckBox(event);
}

void AnalyzerFrame::onNextFrame(wxCommandEvent &WXUNUSED(event)) {
  panel->nextFrame();
  panel->Refresh(false);
}

void AnalyzerFrame::onGotoFrame(wxCommandEvent &WXUNUSED(event)) {}

void AnalyzerFrame::onRestart(wxCommandEvent &WXUNUSED(event)) {}

void AnalyzerFrame::onAbout(wxCommandEvent &WXUNUSED(event)) {
  wxAboutDialogInfo info;
  info.SetName(_("AV1 Bitstream Analyzer"));
  info.SetVersion(_("0.1-beta"));
  info.SetDescription(
      _("This program implements a bitstream analyzer for AV1"));
  info.SetCopyright(
      wxT("(C) 2017 Alliance for Open Media <negge@mozilla.com>"));
  wxAboutBox(info);
}

bool AnalyzerFrame::open(const wxString &path) {
  panel = new AnalyzerPanel(this, path, bit_accounting);
  if (panel->open(path)) {
    SetClientSize(panel->GetSize());
    return true;
  } else {
    delete panel;
    return false;
  }
}

bool AnalyzerFrame::setZoom(int zoom) {
  if (panel->setZoom(zoom)) {
    GetMenuBar()->Enable(wxID_ACTUAL_SIZE, zoom != MIN_ZOOM);
    GetMenuBar()->Enable(wxID_ZOOM_IN, zoom != MAX_ZOOM);
    GetMenuBar()->Enable(wxID_ZOOM_OUT, zoom != MIN_ZOOM);
    SetClientSize(panel->GetSize());
    panel->render();
    panel->Refresh();
    return true;
  }
  return false;
}

void AnalyzerFrame::updateViewMenu() {
  panel->setShowPlane(GetMenuBar()->IsChecked(wxID_SHOW_Y), OD_LUMA_MASK);
  panel->setShowPlane(GetMenuBar()->IsChecked(wxID_SHOW_U), OD_CB_MASK);
  panel->setShowPlane(GetMenuBar()->IsChecked(wxID_SHOW_V), OD_CR_MASK);
  SetClientSize(panel->GetSize());
  panel->render();
  panel->Refresh(false);
}

class Analyzer : public wxApp {
 private:
  AnalyzerFrame *frame;

 public:
  void OnInitCmdLine(wxCmdLineParser &parser);    // NOLINT
  bool OnCmdLineParsed(wxCmdLineParser &parser);  // NOLINT
};

static const wxCmdLineEntryDesc CMD_LINE_DESC[] = {
  { wxCMD_LINE_SWITCH, _("h"), _("help"), _("Display this help and exit."),
    wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
  { wxCMD_LINE_SWITCH, _("a"), _("bit-accounting"), _("Enable bit accounting"),
    wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
  { wxCMD_LINE_PARAM, NULL, NULL, _("input.ivf"), wxCMD_LINE_VAL_STRING,
    wxCMD_LINE_PARAM_OPTIONAL },
  { wxCMD_LINE_NONE }
};

void Analyzer::OnInitCmdLine(wxCmdLineParser &parser) {  // NOLINT
  parser.SetDesc(CMD_LINE_DESC);
  parser.SetSwitchChars(_("-"));
}

bool Analyzer::OnCmdLineParsed(wxCmdLineParser &parser) {  // NOLINT
  bool bit_accounting = parser.Found(_("a"));
  if (bit_accounting && !CONFIG_ACCOUNTING) {
    fprintf(stderr,
            "Bit accounting support not found. "
            "Recompile with:\n./cmake -DCONFIG_ACCOUNTING=1\n");
    return false;
  }
  frame = new AnalyzerFrame(parser.Found(_("a")));
  frame->Show();
  if (parser.GetParamCount() > 0) {
    return frame->open(parser.GetParam(0));
  }
  return true;
}

void usage_exit(void) {
  fprintf(stderr, "uhh\n");
  exit(EXIT_FAILURE);
}

IMPLEMENT_APP(Analyzer)
