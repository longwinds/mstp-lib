
#include "pch.h"
#include "Simulator.h"
#include "Win32/D2DWindow.h"
#include "Bridge.h"
#include "Port.h"

using namespace std;
using namespace D2D1;

class LogArea : public D2DWindow, public ILogArea
{
	typedef D2DWindow base;

	ISelectionPtr const _selection;
	IDWriteTextFormatPtr _textFormat;
	ID2D1SolidColorBrushPtr _windowBrush;
	ID2D1SolidColorBrushPtr _windowTextBrush;
	Bridge* _bridge = nullptr;
	int _selectedPort = -1;
	int _selectedTree = -1;
	vector<const BridgeLogLine*> _lines;
	UINT_PTR _timerId = 0;
	int _animationCurrentLineCount = 0;
	int _animationEndLineCount = 0;
	UINT _animationScrollFramesRemaining = 0;
	int _topLineIndex = 0;
	int _numberOfLinesFitting = 0;
	static constexpr UINT AnimationDurationMilliseconds = 75;
	static constexpr UINT AnimationScrollFramesMax = 10;

public:
	LogArea (HINSTANCE hInstance, HWND hWndParent, const RECT& rect, IDWriteFactory* dWriteFactory, ISelection* selection)
		: base (hInstance, WS_EX_CLIENTEDGE, WS_VISIBLE | WS_CHILD | WS_HSCROLL | WS_VSCROLL, rect, hWndParent, nullptr, dWriteFactory)
		, _selection(selection)
	{
		auto hr = dWriteFactory->CreateTextFormat (L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, 11, L"en-US", &_textFormat); assert(SUCCEEDED(hr));

		_numberOfLinesFitting = CalcNumberOfLinesFitting (_textFormat, GetClientSizeDips().height, dWriteFactory);

		GetRenderTarget()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_WINDOW), &_windowBrush);
		GetRenderTarget()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_WINDOWTEXT), &_windowTextBrush);
		_selection->GetChangedEvent().AddHandler (&OnSelectionChanged, this);
	}

private:
	~LogArea()
	{
		_selection->GetChangedEvent().RemoveHandler (&OnSelectionChanged, this);
		if (_bridge != nullptr)
			_bridge->GetLogLineGeneratedEvent().RemoveHandler (OnLogLineGeneratedStatic, this);
	}

	static void OnSelectionChanged (void* callbackArg, ISelection* selection)
	{
		auto logArea = static_cast<LogArea*>(callbackArg);

		if (selection->GetObjects().size() != 1)
			logArea->SelectBridge(nullptr);
		else
		{
			auto b = dynamic_cast<Bridge*>(selection->GetObjects().front());
			if (b == nullptr)
			{
				auto port = dynamic_cast<Port*>(selection->GetObjects().front());
				if (port != nullptr)
					b = port->GetBridge();
			}

			logArea->SelectBridge(b);
		}
	}

	virtual void Render(ID2D1RenderTarget* dc) const override final
	{
		dc->Clear(GetD2DSystemColor(COLOR_WINDOW));

		auto clientSize = GetClientSizeDips();

		if ((_bridge == nullptr) || _lines.empty())
		{
			static constexpr wchar_t TextNoBridge[] = L"The STP activity log is shown here.\r\nSelect a bridge to see its log.";
			static constexpr wchar_t TextNoEntries[] = L"No log text generated yet.\r\nYou may want to enable STP on the selected bridge.";

			auto oldta = _textFormat->GetTextAlignment();
			_textFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_CENTER);
			IDWriteTextLayoutPtr tl;
			auto text = (_bridge == nullptr) ? TextNoBridge : TextNoEntries;
			auto hr = GetDWriteFactory()->CreateTextLayout (text, (UINT32) wcslen(text), _textFormat, clientSize.width, 10000, &tl); assert(SUCCEEDED(hr));
			_textFormat->SetTextAlignment(oldta);
			DWRITE_TEXT_METRICS metrics;
			tl->GetMetrics (&metrics);
			dc->DrawTextLayout ({ clientSize.width / 2 - metrics.width / 2 - metrics.left, clientSize.height / 2 }, tl, _windowTextBrush);
		}
		else
		{
			wstring_convert<codecvt_utf8<wchar_t>> converter;

			float y = 0;
			float lineHeight = 0;
			for (int lineIndex = _topLineIndex; (lineIndex < _animationCurrentLineCount) && (y < clientSize.height); lineIndex++)
			{
				auto& line = converter.from_bytes(_lines[lineIndex]->text);

				if ((line.length() >= 2) && (line[line.length() - 2] == '\r') && (line[line.length() - 1] == '\n'))
					line.resize (line.length() - 2);

				IDWriteTextLayoutPtr tl;
				auto hr = GetDWriteFactory()->CreateTextLayout (line.c_str(), (UINT32) line.length(), _textFormat, 10000, 10000, &tl); assert(SUCCEEDED(hr));

				if (lineHeight == 0)
				{
					DWRITE_TEXT_METRICS metrics;
					tl->GetMetrics (&metrics);
					lineHeight = metrics.height;
				}

				dc->DrawTextLayout ({ 0, y }, tl, _windowTextBrush, D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
				y += lineHeight;

				if (y >= clientSize.height)
					break;
			}
		}
	}

	static void OnLogLineGeneratedStatic (void* callbackArg, Bridge* b, const BridgeLogLine* ll)
	{
		static_cast<LogArea*>(callbackArg)->OnLogLineGenerated(ll);
	}

	void OnLogLineGenerated (const BridgeLogLine* ll)
	{
		if (((_selectedPort == -1) || (_selectedPort == ll->portIndex))
			&& ((_selectedTree == -1) || (_selectedTree == ll->treeIndex)))
		{
			_lines.push_back(ll);

			bool lastLineVisible = (_topLineIndex + _numberOfLinesFitting >= _animationCurrentLineCount);

			if (!lastLineVisible)
			{
				// The user has scrolled away from the last time. We append the text without doing any scrolling.

				// If the user scrolled away, the animation is supposed to have been stopped.
				assert (_animationCurrentLineCount == _animationEndLineCount);
				assert (_animationScrollFramesRemaining == 0);

				SCROLLINFO si = { sizeof (si) };
				si.fMask = SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL;
				si.nMin = 0;
				si.nMax = (int) _lines.size() - 1;
				si.nPage = _numberOfLinesFitting;
				SetScrollInfo (GetHWnd(), SB_VERT, &si, TRUE);

				_animationCurrentLineCount = (int) _lines.size();
				_animationEndLineCount     = (int) _lines.size();

				InvalidateRect(GetHWnd(), nullptr, FALSE);
			}
			else
			{
				// The last line is visible, meaning that the user didn't scroll away from it.
				// An animation might be pending or not. In any case, we restart it with the new parameters.
				_animationEndLineCount = (int) _lines.size();
				_animationScrollFramesRemaining = AnimationScrollFramesMax;

				if (_timerId != 0)
				{
					KillTimer (GetHWnd(), _timerId);
					_timerId = 0;
				}

				UINT animationFrameLengthMilliseconds = AnimationDurationMilliseconds / AnimationScrollFramesMax;
				_timerId = SetTimer (GetHWnd(), 1, animationFrameLengthMilliseconds, NULL);
			}
		}
	}

	void SelectBridge (Bridge* b)
	{
		if (_bridge != b)
		{
			if (_bridge != nullptr)
			{
				if (_animationScrollFramesRemaining > 0)
					EndAnimation();

				_lines.clear();
				_bridge->GetLogLineGeneratedEvent().RemoveHandler (OnLogLineGeneratedStatic, this);
				_bridge = nullptr;
			}

			_bridge = b;

			if (b != nullptr)
			{
				for (auto& ll : _bridge->GetLogLines())
				{
					if (((_selectedPort == -1) || (_selectedPort == ll->portIndex))
						&& ((_selectedTree == -1) || (_selectedTree == ll->treeIndex)))
					{
						_lines.push_back(ll.get());
					}
				}

				_bridge->GetLogLineGeneratedEvent().AddHandler (OnLogLineGeneratedStatic, this);
			}

			_topLineIndex = max (0, (int) _lines.size() - _numberOfLinesFitting);
			_animationCurrentLineCount = (int) _lines.size();
			_animationEndLineCount     = (int) _lines.size();

			SCROLLINFO si = { sizeof (si) };
			si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
			si.nMin = 0;
			si.nMax = (int) _lines.size() - 1;
			si.nPage = _numberOfLinesFitting;
			si.nPos = _topLineIndex;
			SetScrollInfo (GetHWnd(), SB_VERT, &si, TRUE);

			InvalidateRect (GetHWnd(), nullptr, FALSE);
		}
	}

	virtual optional<LRESULT> WindowProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override final
	{
		if (msg == WM_SIZE)
		{
			base::WindowProc (hwnd, msg, wParam, lParam); // Pass it to the base class first, which stores the client size.
			ProcessWmSize (wParam, lParam);
			return 0;
		}

		if (msg == WM_VSCROLL)
		{
			ProcessWmVScroll (wParam, lParam);
			return 0;
		}

		if ((msg == WM_TIMER) && (_timerId != 0) && (wParam == _timerId))
		{
			ProcessAnimationTimer();
			return 0;
		}

		if (msg == WM_MOUSEWHEEL)
		{
			ProcessWmMouseWheel (wParam, lParam);
			return 0;
		}

		return base::WindowProc (hwnd, msg, wParam, lParam);
	}

	void ProcessAnimationTimer()
	{
		assert (_animationEndLineCount != _animationCurrentLineCount);
		assert (_animationScrollFramesRemaining != 0);

		int linesToAddInThisAnimationFrame = (_animationEndLineCount - _animationCurrentLineCount) / _animationScrollFramesRemaining;
		_animationCurrentLineCount += linesToAddInThisAnimationFrame;

		if (_animationCurrentLineCount <= _numberOfLinesFitting)
			_topLineIndex = 0;
		else
			_topLineIndex = _animationCurrentLineCount - _numberOfLinesFitting;

		InvalidateRect (GetHWnd(), nullptr, FALSE);

		// Need to set SIF_DISABLENOSCROLL due to what seems like a Windows bug:
		// GetScrollInfo returns garbage if called right after SetScrollInfo, if SetScrollInfo made the scroll bar change from invisible to visible,
		SCROLLINFO si = { sizeof (si) };
		si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL;
		si.nMin = 0;
		si.nMax = _animationCurrentLineCount - 1;
		si.nPage = _numberOfLinesFitting;
		si.nPos = _topLineIndex;
		SetScrollInfo (GetHWnd(), SB_VERT, &si, TRUE);

		if (_timerId != 0)
		{
			KillTimer (GetHWnd(), _timerId);
			_timerId = 0;
		}

		_animationScrollFramesRemaining--;
		if (_animationScrollFramesRemaining > 0)
		{
			UINT animationFrameLengthMilliseconds = AnimationDurationMilliseconds / AnimationScrollFramesMax;
			_timerId = SetTimer (GetHWnd(), (UINT_PTR) 1, animationFrameLengthMilliseconds, NULL); assert (_timerId != 0);
		}
	}

	static int CalcNumberOfLinesFitting (IDWriteTextFormat* textFormat, float clientHeightDips, IDWriteFactory* dWriteFactory)
	{
		IDWriteTextLayoutPtr tl;
		auto hr = dWriteFactory->CreateTextLayout (L"A", 1, textFormat, 1000, 1000, &tl); assert(SUCCEEDED(hr));
		DWRITE_TEXT_METRICS metrics;
		hr = tl->GetMetrics(&metrics);
		int numberOfLinesFitting = (int) floor(clientHeightDips / metrics.height);
		return numberOfLinesFitting;
	}

	void ProcessWmSize (WPARAM wParam, LPARAM lParam)
	{
		bool isLastLineVisible = (_topLineIndex + _numberOfLinesFitting >= _animationCurrentLineCount);

		if (_animationScrollFramesRemaining > 0)
		{
			// Scroll animation is in progress. Complete the animation, for now without taking into account the new size of the client area.
			EndAnimation();

			// ???
			if (_animationCurrentLineCount > _numberOfLinesFitting)
				_topLineIndex = _animationCurrentLineCount - _numberOfLinesFitting;
			else
				_topLineIndex = 0;
		}

		int newNumberOfLinesFitting = CalcNumberOfLinesFitting (_textFormat, GetClientSizeDips().height, GetDWriteFactory());
		if (_numberOfLinesFitting != newNumberOfLinesFitting)
		{
			_numberOfLinesFitting = newNumberOfLinesFitting;

			if (isLastLineVisible)
			{
				// We must keep the last line at the bottom of the client area.

				if (_animationCurrentLineCount > _numberOfLinesFitting)
					_topLineIndex = _animationCurrentLineCount - _numberOfLinesFitting;
				else
					_topLineIndex = 0;

				InvalidateRect (GetHWnd(), nullptr, FALSE);
			}
		}

		// Need to set SIF_DISABLENOSCROLL due to what seems like a Windows bug:
		// GetScrollInfo returns garbage if called right after SetScrollInfo, if SetScrollInfo made the scroll bar change from invisible to visible,

		// TODO: fix this
		SCROLLINFO si = { sizeof (si) };
		si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
		si.nMin = 0;
		si.nMax = _animationCurrentLineCount - 1;
		si.nPos = _topLineIndex;
		si.nPage = _numberOfLinesFitting;
		SetScrollInfo (GetHWnd(), SB_VERT, &si, TRUE);
	}

	void EndAnimation()
	{
		assert (_animationScrollFramesRemaining > 0);

		// Scroll animation is in progress. Finalize it.
		assert (_animationEndLineCount > _animationCurrentLineCount);
		assert (_timerId != 0);
		BOOL bRes = KillTimer (GetHWnd(), _timerId); assert(bRes);
		_timerId = 0;

		_animationCurrentLineCount = _animationEndLineCount;
		_animationScrollFramesRemaining = 0;
		InvalidateRect (GetHWnd(), nullptr, FALSE);
	}

	void ProcessUserScroll (int newTopLineIndex)
	{
		if (_topLineIndex != newTopLineIndex)
		{
			_topLineIndex = newTopLineIndex;
			InvalidateRect (GetHWnd(), nullptr, FALSE);
			SetScrollPos (GetHWnd(), SB_VERT, _topLineIndex, TRUE);
		}
	}

	void ProcessWmVScroll (WPARAM wParam, LPARAM lParam)
	{
		if (_animationScrollFramesRemaining > 0)
			EndAnimation();

		int newTopLineIndex = _topLineIndex;
		switch (LOWORD(wParam))
		{
			case SB_LINEUP:
				newTopLineIndex = max (_topLineIndex - 1, 0);
				break;

			case SB_PAGEUP:
				newTopLineIndex = max (_topLineIndex - _numberOfLinesFitting, 0);
				break;

			case SB_LINEDOWN:
				newTopLineIndex = _topLineIndex + min(_animationEndLineCount - (_topLineIndex + _numberOfLinesFitting), 1);
				break;

			case SB_PAGEDOWN:
				newTopLineIndex = _topLineIndex + min(_animationEndLineCount - (_topLineIndex + _numberOfLinesFitting), _numberOfLinesFitting);
				break;

			case SB_THUMBTRACK:
				newTopLineIndex = (int) HIWORD(wParam);
				break;
		}

		ProcessUserScroll (newTopLineIndex);
	}

	void ProcessWmMouseWheel (WPARAM wParam, LPARAM lParam)
	{
		WORD fwKeys = GET_KEYSTATE_WPARAM(wParam);
		short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);

		if (_animationScrollFramesRemaining > 0)
			EndAnimation();

		UINT scrollLines;
		SystemParametersInfo (SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0);
		int linesToScroll = -(int) zDelta * (int) scrollLines / WHEEL_DELTA;

		int newTopLineIndex;
		if (linesToScroll < 0)
			newTopLineIndex = max (_topLineIndex + linesToScroll, 0);
		else
			newTopLineIndex = _topLineIndex + min(_animationEndLineCount - (_topLineIndex + _numberOfLinesFitting), linesToScroll);

		ProcessUserScroll (newTopLineIndex);
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return base::QueryInterface(riid, ppvObject); }
	virtual ULONG STDMETHODCALLTYPE AddRef() override { return base::AddRef(); }
	virtual ULONG STDMETHODCALLTYPE Release() override { return base::Release(); }

	virtual HWND GetHWnd() const override { return base::GetHWnd(); }
};

template<typename... Args>
static ILogAreaPtr Create (Args... args)
{
	return ILogAreaPtr (new LogArea(std::forward<Args>(args)...), false);
}

extern const LogAreaFactory logAreaFactory = &Create;
