
#include "pch.h"
#include "Port.h"
#include "Bridge.h"
#include "Win32/UtilityFunctions.h"

using namespace std;
using namespace D2D1;

Port::Port (Bridge* bridge, unsigned int portIndex, Side side, float offset)
	: _bridge(bridge), _portIndex(portIndex), _side(side), _offset(offset)
{
	for (unsigned int treeIndex = 0; treeIndex < (unsigned int) bridge->GetTrees().size(); treeIndex++)
		_trees.push_back (unique_ptr<PortTree>(new PortTree(this, treeIndex)));
}

D2D1_POINT_2F Port::GetCPLocation() const
{
	auto bounds = _bridge->GetBounds();

	if (_side == Side::Left)
		return Point2F (bounds.left - ExteriorHeight, bounds.top + _offset);

	if (_side == Side::Right)
		return Point2F (bounds.right + ExteriorHeight, bounds.top + _offset);

	if (_side == Side::Top)
		return Point2F (bounds.left + _offset, bounds.top - ExteriorHeight);

	if (_side == Side::Bottom)
		return Point2F (bounds.left + _offset, bounds.bottom + ExteriorHeight);

	assert(false); // not implemented
	return { 0, 0 };
}

Matrix3x2F Port::GetPortTransform() const
{
	if (_side == Side::Left)
	{
		//portTransform = Matrix3x2F::Rotation (90, Point2F (0, 0)) * Matrix3x2F::Translation (bridgeRect.left, bridgeRect.top + port->GetOffset ());
		// The above calculation is correct but slow. Let's assign the matrix members directly.
		return { 0, 1, -1, 0, _bridge->GetLeft(), _bridge->GetTop() + _offset};
	}
	else if (_side == Side::Right)
	{
		//portTransform = Matrix3x2F::Rotation (270, Point2F (0, 0)) * Matrix3x2F::Translation (bridgeRect.right, bridgeRect.top + port->GetOffset ());
		return { 0, -1, 1, 0, _bridge->GetRight(), _bridge->GetTop() + _offset };
	}
	else if (_side == Side::Top)
	{
		//portTransform = Matrix3x2F::Rotation (180, Point2F (0, 0)) * Matrix3x2F::Translation (bridgeRect.left + port->GetOffset (), bridgeRect.top);
		return { -1, 0, 0, -1, _bridge->GetLeft() + _offset, _bridge->GetTop() };
	}
	else //if (_side == Side::Bottom)
	{
		//portTransform = Matrix3x2F::Translation (bridgeRect.left + port->GetOffset (), bridgeRect.bottom);
		return { 1, 0, 0, 1, _bridge->GetLeft() + _offset, _bridge->GetBottom() };
	}
}

// static
void Port::RenderExteriorNonStpPort (ID2D1RenderTarget* dc, const DrawingObjects& dos, bool macOperational)
{
	auto brush = macOperational ? dos._brushForwarding : dos._brushDiscardingPort;
	dc->DrawLine (Point2F (0, 0), Point2F (0, ExteriorHeight), brush, 2);
}

// static
void Port::RenderExteriorStpPort (ID2D1RenderTarget* dc, const DrawingObjects& dos, STP_PORT_ROLE role, bool learning, bool forwarding, bool operEdge)
{
	static constexpr float circleDiameter = min (ExteriorHeight / 2, ExteriorWidth);

	static constexpr float edw = ExteriorWidth;
	static constexpr float edh = ExteriorHeight;

	static constexpr float discardingFirstHorizontalLineY = circleDiameter + (edh - circleDiameter) / 3;
	static constexpr float discardingSecondHorizontalLineY = circleDiameter + (edh - circleDiameter) * 2 / 3;
	static constexpr float learningHorizontalLineY = circleDiameter + (edh - circleDiameter) / 2;

	static constexpr float dfhly = discardingFirstHorizontalLineY;
	static constexpr float dshly = discardingSecondHorizontalLineY;

	static const D2D1_ELLIPSE ellipseFill = { Point2F (0, circleDiameter / 2), circleDiameter / 2 + 0.5f, circleDiameter / 2 + 0.5f };
	static const D2D1_ELLIPSE ellipseDraw = { Point2F (0, circleDiameter / 2), circleDiameter / 2 - 0.5f, circleDiameter / 2 - 0.5f};

	auto oldaa = dc->GetAntialiasMode();

	if (role == STP_PORT_ROLE_DISABLED)
	{
		// disabled
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->DrawLine (Point2F (-edw / 2, edh / 3), Point2F (edw / 2, edh * 2 / 3), dos._brushDiscardingPort);
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) && !learning && !forwarding)
	{
		// designated discarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->FillEllipse (&ellipseFill, dos._brushDiscardingPort);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), dos._brushDiscardingPort);
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) && learning && !forwarding)
	{
		// designated learning
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->FillEllipse (&ellipseFill, dos._brushLearningPort);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushLearningPort, 2);
		dc->DrawLine (Point2F (-edw / 2, learningHorizontalLineY), Point2F (edw / 2, learningHorizontalLineY), dos._brushLearningPort);
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) && learning && forwarding && !operEdge)
	{
		// designated forwarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->FillEllipse (&ellipseFill, dos._brushForwarding);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushForwarding, 2);
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) && learning && forwarding && operEdge)
	{
		// designated forwarding operEdge
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->FillEllipse (&ellipseFill, dos._brushForwarding);
		static constexpr D2D1_POINT_2F points[] =
		{
			{ 0, circleDiameter },
			{ -edw / 2 + 1, circleDiameter + (edh - circleDiameter) / 2 },
			{ 0, edh },
			{ edw / 2 - 1, circleDiameter + (edh - circleDiameter) / 2 },
		};

		dc->DrawLine (points[0], points[1], dos._brushForwarding, 2);
		dc->DrawLine (points[1], points[2], dos._brushForwarding, 2);
		dc->DrawLine (points[2], points[3], dos._brushForwarding, 2);
		dc->DrawLine (points[3], points[0], dos._brushForwarding, 2);
	}
	else if (((role == STP_PORT_ROLE_ROOT) || (role == STP_PORT_ROLE_MASTER)) && !learning && !forwarding)
	{
		// root or master discarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->DrawEllipse (&ellipseDraw, dos._brushDiscardingPort, 2);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), dos._brushDiscardingPort);
	}
	else if (((role == STP_PORT_ROLE_ROOT) || (role == STP_PORT_ROLE_MASTER)) && learning && !forwarding)
	{
		// root or master learning
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->DrawEllipse (&ellipseDraw, dos._brushLearningPort, 2);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushLearningPort, 2);
		dc->DrawLine (Point2F (-edw / 2, learningHorizontalLineY), Point2F (edw / 2, learningHorizontalLineY), dos._brushLearningPort);
	}
	else if (((role == STP_PORT_ROLE_ROOT) || (role == STP_PORT_ROLE_MASTER)) && learning && forwarding)
	{
		// root or master forwarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->DrawEllipse (&ellipseDraw, dos._brushForwarding, 2);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushForwarding, 2);
	}
	else if ((role == STP_PORT_ROLE_ALTERNATE) && !learning && !forwarding)
	{
		// Alternate discarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), dos._brushDiscardingPort);
	}
	else if ((role == STP_PORT_ROLE_ALTERNATE) && learning && !forwarding)
	{
		// Alternate learning
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushLearningPort, 2);
		dc->DrawLine (Point2F (-edw / 2, learningHorizontalLineY), Point2F (edw / 2, learningHorizontalLineY), dos._brushLearningPort);
	}
	else if ((role == STP_PORT_ROLE_BACKUP) && !learning && !forwarding)
	{
		// Backup discarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->DrawLine (Point2F (-edw / 2, dfhly / 2), Point2F (edw / 2, dfhly / 2), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), dos._brushDiscardingPort);
	}
	else if (role == STP_PORT_ROLE_UNKNOWN)
	{
		// Undefined
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->DrawText (L"?", 1, dos._regularTextFormat, { 2, 0, 20, 20 }, dos._brushDiscardingPort, D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
	}
	else
		assert(false); // not implemented

	dc->SetAntialiasMode(oldaa);
}

void Port::Render (ID2D1RenderTarget* rt, const DrawingObjects& dos, unsigned int vlanNumber) const
{
	D2D1_MATRIX_3X2_F oldtr;
	rt->GetTransform(&oldtr);
	rt->SetTransform (GetPortTransform() * oldtr);

	// Draw the exterior of the port.
	float interiorPortOutlineWidth = OutlineWidth;
	auto b = _bridge->GetStpBridge();
	auto treeIndex  = STP_GetTreeIndexFromVlanNumber(b, vlanNumber);
	if (STP_IsBridgeStarted (b))
	{
		auto role       = STP_GetPortRole (b, (unsigned int) _portIndex, treeIndex);
		auto learning   = STP_GetPortLearning (b, (unsigned int) _portIndex, treeIndex);
		auto forwarding = STP_GetPortForwarding (b, (unsigned int) _portIndex, treeIndex);
		auto operEdge   = STP_GetPortOperEdge (b, (unsigned int) _portIndex);
		RenderExteriorStpPort (rt, dos, role, learning, forwarding, operEdge);

		if (role == STP_PORT_ROLE_ROOT)
			interiorPortOutlineWidth *= 2;
	}
	else
		RenderExteriorNonStpPort(rt, dos, GetMacOperational());

	// Draw the interior of the port.
	auto portRect = D2D1_RECT_F { -InteriorWidth / 2, -InteriorDepth, InteriorWidth / 2, 0 };
	InflateRect (&portRect, -interiorPortOutlineWidth / 2);
	rt->FillRectangle (&portRect, GetMacOperational() ? dos._poweredFillBrush : dos._unpoweredBrush);
	rt->DrawRectangle (&portRect, dos._brushWindowText, interiorPortOutlineWidth);

	IDWriteTextFormatPtr format;
	auto hr = dos._dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 9, L"en-US", &format); assert(SUCCEEDED(hr));
	IDWriteTextLayoutPtr layout;
	wstringstream ss;
	ss << setfill(L'0') << setw(4) << hex << STP_GetPortIdentifier(b, (unsigned int) _portIndex, treeIndex);
	auto portIdText = ss.str();
	hr = dos._dWriteFactory->CreateTextLayout (portIdText.c_str(), (UINT32) portIdText.length(), format, 10000, 10000, &layout); assert(SUCCEEDED(hr));
	DWRITE_TEXT_METRICS metrics;
	hr = layout->GetMetrics(&metrics); assert(SUCCEEDED(hr));
	DWRITE_LINE_METRICS lineMetrics;
	UINT32 actualLineCount;
	hr = layout->GetLineMetrics(&lineMetrics, 1, &actualLineCount); assert(SUCCEEDED(hr));
	rt->DrawTextLayout ({ -metrics.width / 2, -lineMetrics.baseline - OutlineWidth * 2 - 1}, layout, dos._brushWindowText);

	rt->SetTransform (&oldtr);
}

D2D1_RECT_F Port::GetInnerOuterRect() const
{
	auto tl = D2D1_POINT_2F { -InteriorWidth / 2, -InteriorDepth };
	auto br = D2D1_POINT_2F { InteriorWidth / 2, ExteriorHeight };
	auto tr = GetPortTransform();
	tl = tr.TransformPoint(tl);
	br = tr.TransformPoint(br);
	return { min(tl.x, br.x), min (tl.y, br.y), max(tl.x, br.x), max(tl.y, br.y) };
}

void Port::RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const
{
	auto ir = GetInnerOuterRect();

	auto oldaa = rt->GetAntialiasMode();
	rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

	auto lt = zoomable->GetDLocationFromWLocation ({ ir.left, ir.top });
	auto rb = zoomable->GetDLocationFromWLocation ({ ir.right, ir.bottom });
	rt->DrawRectangle ({ lt.x - 10, lt.y - 10, rb.x + 10, rb.y + 10 }, dos._brushHighlight, 2, dos._strokeStyleSelectionRect);

	rt->SetAntialiasMode(oldaa);
}

bool Port::HitTestCP (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) const
{
	auto cpWLocation = GetCPLocation();
	auto cpDLocation = zoomable->GetDLocationFromWLocation(cpWLocation);

	return (abs (cpDLocation.x - dLocation.x) <= tolerance)
		&& (abs (cpDLocation.y - dLocation.y) <= tolerance);
}

bool Port::HitTestInnerOuter (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) const
{
	auto ir = GetInnerOuterRect();
	auto lt = zoomable->GetDLocationFromWLocation ({ ir.left, ir.top });
	auto rb = zoomable->GetDLocationFromWLocation ({ ir.right, ir.bottom });
	return (dLocation.x >= lt.x) && (dLocation.y >= lt.y) && (dLocation.x < rb.x) && (dLocation.y < rb.y);
}

RenderableObject::HTResult Port::HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance)
{
	if (HitTestCP (zoomable, dLocation, tolerance))
		return { this, HTCodeCP };

	if (HitTestInnerOuter (zoomable, dLocation, tolerance))
		return { this, HTCodeInnerOuter };

	return { };
}

bool Port::IsForwarding (unsigned int vlanNumber) const
{
	auto stpb = _bridge->GetStpBridge();
	if (!STP_IsBridgeStarted(stpb))
		return true;

	auto treeIndex = STP_GetTreeIndexFromVlanNumber(stpb, vlanNumber);
	return STP_GetPortForwarding (stpb, (unsigned int) _portIndex, treeIndex);
}

void Port::SetSideAndOffset (Side side, float offset)
{
	if ((_side != side) || (_offset != offset))
	{
		_side = side;
		_offset = offset;
		InvalidateEvent::InvokeHandlers (this, this);
	}
}

bool Port::GetMacOperational() const
{
	return _missedLinkPulseCounter < MissedLinkPulseCounterMax;
}

bool Port::GetAutoEdge() const
{
	return (bool) STP_GetPortAutoEdge (_bridge->GetStpBridge(), (unsigned int) _portIndex);
}

void Port::SetAutoEdge (bool autoEdge)
{
	STP_SetPortAutoEdge (_bridge->GetStpBridge(), (unsigned int) _portIndex, autoEdge, (unsigned int) GetMessageTime());
}

bool Port::GetAdminEdge() const
{
	return (bool) STP_GetPortAdminEdge (_bridge->GetStpBridge(), (unsigned int) _portIndex);
}

void Port::SetAdminEdge (bool adminEdge)
{
	STP_SetPortAdminEdge (_bridge->GetStpBridge(), (unsigned int) _portIndex, adminEdge, (unsigned int) GetMessageTime());
}

static const _bstr_t PortString = "Port";
static const _bstr_t SideString = L"Side";
static const _bstr_t OffsetString = L"Offset";
static const _bstr_t AutoEdgeString = L"AutoEdge";
static const _bstr_t AdminEdgeString = L"AdminEdge";
static const _bstr_t PortTreesString = "PortTrees";

IXMLDOMElementPtr Port::Serialize (IXMLDOMDocument3* doc) const
{
	IXMLDOMElementPtr portElement;
	auto hr = doc->createElement (PortString, &portElement); assert(SUCCEEDED(hr));
	portElement->setAttribute (SideString, _variant_t (GetEnumName (SideNVPs, (int) _side)));
	portElement->setAttribute (OffsetString, _variant_t (_offset));
	portElement->setAttribute (AutoEdgeString, _variant_t (GetAutoEdge() ? L"True" : L"False"));
	portElement->setAttribute (AdminEdgeString, _variant_t (GetAdminEdge() ? L"True" : L"False"));

	IXMLDOMElementPtr portTreesElement;
	hr = doc->createElement (PortTreesString, &portTreesElement); assert(SUCCEEDED(hr));
	hr = portElement->appendChild(portTreesElement, nullptr); assert(SUCCEEDED(hr));
	for (size_t treeIndex = 0; treeIndex < _trees.size(); treeIndex++)
	{
		IXMLDOMElementPtr portTreeElement;
		hr = _trees.at(treeIndex)->Serialize (doc, portTreeElement); assert(SUCCEEDED(hr));
		hr = portTreesElement->appendChild (portTreeElement, nullptr); assert(SUCCEEDED(hr));
	}

	return portElement;
}

HRESULT Port::Deserialize (IXMLDOMElement* portElement)
{
	_variant_t value;
	auto hr = portElement->getAttribute (SideString, &value);
	if (FAILED(hr))
		return hr;
	if (value.vt == VT_BSTR)
		_side = (Side) GetEnumValue (SideNVPs, value.bstrVal);

	hr = portElement->getAttribute (OffsetString, &value);
	if (FAILED(hr))
		return hr;
	if (value.vt == VT_BSTR)
		_offset = wcstof (value.bstrVal, nullptr);

	hr = portElement->getAttribute (AutoEdgeString, &value);
	if (FAILED(hr))
		return hr;
	if (value.vt == VT_BSTR)
		SetAutoEdge (_wcsicmp (value.bstrVal, L"True") == 0);

	hr = portElement->getAttribute (AdminEdgeString, &value);
	if (FAILED(hr))
		return hr;
	if (value.vt == VT_BSTR)
		SetAdminEdge (_wcsicmp (value.bstrVal, L"True") == 0);

	IXMLDOMNodePtr portTreesNode;
	hr = portElement->selectSingleNode(PortTreesString, &portTreesNode);
	if (SUCCEEDED(hr) && (portTreesNode != nullptr))
	{
		IXMLDOMNodeListPtr portTreeNodes;
		hr = portTreesNode->get_childNodes(&portTreeNodes); assert(SUCCEEDED(hr));

		for (size_t treeIndex = 0; treeIndex < _trees.size(); treeIndex++)
		{
			IXMLDOMNodePtr portTreeNode;
			hr = portTreeNodes->get_item((long) treeIndex, &portTreeNode); assert(SUCCEEDED(hr));
			IXMLDOMElementPtr portTreeElement = portTreeNode;
			hr = _trees.at(treeIndex)->Deserialize(portTreeElement); assert(SUCCEEDED(hr));
		}
	}

	return S_OK;
}

const TypedProperty<bool> Port::AutoEdge
(
	L"AutoEdge",
	nullptr,
	static_cast<TypedProperty<bool>::Getter>(&GetAutoEdge),
	static_cast<TypedProperty<bool>::Setter>(&SetAutoEdge)
);

const TypedProperty<bool> Port::AdminEdge
(
	L"AdminEdge",
	nullptr,
	static_cast<TypedProperty<bool>::Getter>(&GetAdminEdge),
	static_cast<TypedProperty<bool>::Setter>(&SetAdminEdge)
);

const TypedProperty<bool> Port::MacOperational
(
	L"MAC_Operational",
	nullptr,
	static_cast<TypedProperty<bool>::Getter>(&GetMacOperational),
	nullptr
);

const PropertyOrGroup* const Port::Properties[] =
{
	&AutoEdge,
	&AdminEdge,
	&MacOperational,
	nullptr
};

