
#include "pch.h"
#include "EditState.h"
#include "Bridge.h"
#include "Wire.h"
#include "Port.h"

using namespace std;

static size_t nextWireIndex = 1;

class CreateWireES : public EditState
{
	typedef EditState base;

	unique_ptr<Wire> _wire;
	POINT _firstDownLocation;

	enum SubState
	{
		WaitingFirstDown,
		WaitingFirstUp,
		WaitingSecondDown,
		WaitingSecondUp,
		Done,
	};

	SubState _subState = WaitingFirstDown;

public:
	using base::base;

	virtual void OnMouseDown (const MouseLocation& location, MouseButton button) override final
	{
		if (button != MouseButton::Left)
			return;

		if (_subState == WaitingFirstDown)
		{
			auto fromPort = _pw->GetEditArea()->GetCPAt(location.d, SnapDistance);
			if (fromPort != nullptr)
			{
				_firstDownLocation = location.pt;
				_wire.reset (new Wire());
				_wire->SetP0 (fromPort);
				_wire->SetP1 (fromPort->GetCPLocation());
				_wire->SetDebugName ((string("Wire") + to_string(nextWireIndex++)).c_str());
				_subState  = WaitingFirstUp;
			}
		}
		else if (_subState == WaitingSecondDown)
		{
			throw not_implemented_exception();
		}
	}

	virtual void OnMouseMove (const MouseLocation& location) override final
	{
		if (_subState == WaitingFirstDown)
			return;

		auto port = _pw->GetEditArea()->GetCPAt (location.d, SnapDistance);
		if (port != nullptr)
			_wire->SetP1(port);
		else
			_wire->SetP1(location.w);
		::InvalidateRect (_pw->GetEditArea()->GetHWnd(), nullptr, FALSE);

		if (_subState == WaitingFirstUp)
		{
			RECT rc = { _firstDownLocation.x, _firstDownLocation.y, _firstDownLocation.x, _firstDownLocation.y };
			InflateRect (&rc, GetSystemMetrics(SM_CXDRAG), GetSystemMetrics(SM_CYDRAG));
			if (!PtInRect(&rc, location.pt))
				_subState = WaitingSecondUp;
		}
	}

	virtual void OnMouseUp (const MouseLocation& location, MouseButton button) override final
	{
		if (_subState == WaitingSecondUp)
		{
			if (holds_alternative<ConnectedWireEnd>(_wire->GetP1()))
			{
				struct CreateAction : public EditAction
				{
					IProject* const _project;
					unique_ptr<Wire> _wire;
					size_t _insertIndex;
					CreateAction (IProject* project, unique_ptr<Wire>&& wire) : _project(project), _wire(move(wire)) { }
					virtual void Redo() { _insertIndex = _project->GetWires().size(); _project->InsertWire(_insertIndex, move(_wire)); }
					virtual void Undo() { _wire = _project->RemoveWire(_insertIndex); }
				};

				Wire* w = _wire.get();
				_actionList->PerformAndAddUserAction (L"Create Wire", make_unique<CreateAction>(_pw->GetProject(), move(_wire)));
				_selection->Select(w);
				_subState = Done;
			}
		}
	}

	virtual std::optional<LRESULT> OnKeyDown (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_subState = Done;
			::InvalidateRect (_pw->GetEditArea()->GetHWnd(), nullptr, FALSE);
			return 0;
		}

		return nullopt;
	}

	virtual void Render (ID2D1RenderTarget* rt) override final
	{
		D2D1_MATRIX_3X2_F oldtr;
		rt->GetTransform(&oldtr);
		rt->SetTransform(_pw->GetEditArea()->GetZoomTransform());
		_wire->Render(rt, _pw->GetEditArea()->GetDrawingObjects(), _pw->GetSelectedVlanNumber());
		rt->SetTransform(&oldtr);

		if (holds_alternative<ConnectedWireEnd>(_wire->GetP1()))
			_pw->GetEditArea()->RenderSnapRect (rt, get<ConnectedWireEnd>(_wire->GetP1())->GetCPLocation());
	}

	virtual bool Completed() const override final
	{
		return _subState == Done;
	}

	virtual HCURSOR GetCursor() const override final { return LoadCursor(nullptr, IDC_CROSS); }
};

std::unique_ptr<EditState> CreateStateCreateWire (const EditStateDeps& deps)  { return unique_ptr<EditState>(new CreateWireES(deps)); }
