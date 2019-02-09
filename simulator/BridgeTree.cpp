
#include "pch.h"
#include "BridgeTree.h"
#include "Bridge.h"

using namespace std;
using namespace edge;

static const _bstr_t BridgeTreeString = "BridgeTree";
static const _bstr_t TreeIndexString = "TreeIndex";
static const _bstr_t BridgePriorityString = "BridgePriority";

HRESULT BridgeTree::Serialize (IXMLDOMDocument3* doc, com_ptr<IXMLDOMElement>& elementOut) const
{
	com_ptr<IXMLDOMElement> bridgeTreeElement;
	auto hr = doc->createElement (BridgeTreeString, &bridgeTreeElement); assert(SUCCEEDED(hr));
	bridgeTreeElement->setAttribute (TreeIndexString, _variant_t(_treeIndex));
	bridgeTreeElement->setAttribute (BridgePriorityString, _variant_t(bridge_priority()));
	elementOut = std::move(bridgeTreeElement);
	return S_OK;
}

HRESULT BridgeTree::Deserialize (IXMLDOMElement* bridgeTreeElement)
{
	_variant_t value;
	auto hr = bridgeTreeElement->getAttribute (BridgePriorityString, &value);
	if (FAILED(hr))
		return hr;
	if (value.vt == VT_BSTR)
		set_bridge_priority (wcstol (value.bstrVal, nullptr, 10));
	
	return S_OK;
}

uint32_t BridgeTree::bridge_priority() const
{
	return STP_GetBridgePriority (_parent->stp_bridge(), _treeIndex);
}

void BridgeTree::set_bridge_priority (uint32_t priority)
{
	STP_SetBridgePriority (_parent->stp_bridge(), _treeIndex, (unsigned short) priority, GetMessageTime());
}

static constexpr char StpDisabledString[] = "(STP disabled)";

array<unsigned char, 36> BridgeTree::root_priorty_vector() const
{
	array<unsigned char, 36> prioVector;
	STP_GetRootPriorityVector(_parent->stp_bridge(), _treeIndex, prioVector.data());
	return prioVector;
}

std::string BridgeTree::root_bridge_id() const
{
	if (!STP_IsBridgeStarted (_parent->stp_bridge()))
		return StpDisabledString;

	auto rpv = root_priorty_vector();
	stringstream ss;
	ss << uppercase << setfill('0') << hex
		<< setw(2) << (int)rpv[0] << setw(2) << (int)rpv[1] << "."
		<< setw(2) << (int)rpv[2] << setw(2) << (int)rpv[3] << setw(2) << (int)rpv[4]
		<< setw(2) << (int)rpv[5] << setw(2) << (int)rpv[6] << setw(2) << (int)rpv[7];
	return ss.str();
}

std::string BridgeTree::GetExternalRootPathCost() const
{
	if (!STP_IsBridgeStarted(_parent->stp_bridge()))
		return StpDisabledString;

	auto rpv = root_priorty_vector();
	auto cost = ((uint32_t) rpv[8] << 24) | ((uint32_t) rpv[9] << 16) | ((uint32_t) rpv[10] << 8) | rpv[11];
	return to_string (cost);
}

std::string BridgeTree::GetRegionalRootBridgeId() const
{
	if (!STP_IsBridgeStarted (_parent->stp_bridge()))
		return StpDisabledString;

	auto rpv = root_priorty_vector();
	stringstream ss;
	ss << uppercase << setfill('0') << hex
		<< setw(2) << rpv[12] << setw(2) << rpv[13] << "."
		<< setw(2) << rpv[14] << setw(2) << rpv[15] << setw(2) << rpv[16]
		<< setw(2) << rpv[17] << setw(2) << rpv[18] << setw(2) << rpv[19];
	return ss.str();
}

std::string BridgeTree::GetInternalRootPathCost() const
{
	if (!STP_IsBridgeStarted(_parent->stp_bridge()))
		return StpDisabledString;

	auto rpv = root_priorty_vector();
	auto cost = ((uint32_t) rpv[20] << 24) | ((uint32_t) rpv[21] << 16) | ((uint32_t) rpv[22] << 8) | rpv[23];
	return to_string(cost);
}

std::string BridgeTree::GetDesignatedBridgeId() const
{
	if (!STP_IsBridgeStarted(_parent->stp_bridge()))
		return StpDisabledString;

	auto rpv = root_priorty_vector();
	stringstream ss;
	ss << uppercase << setfill('0') << hex
		<< setw(2) << rpv[24] << setw(2) << rpv[25] << "."
		<< setw(2) << rpv[26] << setw(2) << rpv[27] << setw(2) << rpv[28]
		<< setw(2) << rpv[29] << setw(2) << rpv[30] << setw(2) << rpv[31];
	return ss.str();
}

std::string BridgeTree::GetDesignatedPortId() const
{
	if (!STP_IsBridgeStarted(_parent->stp_bridge()))
		return StpDisabledString;

	auto rpv = root_priorty_vector();
	stringstream ss;
	ss << uppercase << setfill('0') << hex << setw(2) << rpv[32] << setw(2) << rpv[33];
	return ss.str();
}

std::string BridgeTree::GetReceivingPortId() const
{
	if (!STP_IsBridgeStarted(_parent->stp_bridge()))
		return StpDisabledString;

	auto rpv = root_priorty_vector();
	stringstream ss;
	ss << uppercase << setfill('0') << hex << setw(2) << rpv[34] << setw(2) << rpv[35];
	return ss.str();
}

uint32_t BridgeTree::hello_time() const
{
	unsigned short ht;
	STP_GetRootTimes(_parent->stp_bridge(), _treeIndex, nullptr, &ht, nullptr, nullptr, nullptr);
	return ht;
}

uint32_t BridgeTree::max_age() const
{
	unsigned short ma;
	STP_GetRootTimes(_parent->stp_bridge(), _treeIndex, nullptr, nullptr, &ma, nullptr, nullptr);
	return ma;
}

uint32_t BridgeTree::forward_delay() const
{
	unsigned short fd;
	STP_GetRootTimes(_parent->stp_bridge(), _treeIndex, &fd, nullptr, nullptr, nullptr, nullptr);
	return fd;
}

uint32_t BridgeTree::message_age() const
{
	unsigned short ma;
	STP_GetRootTimes(_parent->stp_bridge(), _treeIndex, nullptr, nullptr, nullptr, &ma, nullptr);
	return ma;
}

uint32_t BridgeTree::remaining_hops() const
{
	unsigned char rh;
	STP_GetRootTimes(_parent->stp_bridge(), _treeIndex, nullptr, nullptr, nullptr, nullptr, &rh);
	return rh;
}

// ============================================================================

static const edge::property_group root_times_group = { 5, "Root Times" };

const NVP bridge_priority_nvps[] =
{
	{ "1000 (4096 dec)", 0x1000 },
	{ "2000 (8192 dec)", 0x2000 },
	{ "3000 (12288 dec)", 0x3000 },
	{ "4000 (16384 dec)", 0x4000 },
	{ "5000 (20480 dec)", 0x5000 },
	{ "6000 (24576 dec)", 0x6000 },
	{ "7000 (28672 dec)", 0x7000 },
	{ "8000 (32768 dec)", 0x8000 },
	{ "9000 (36864 dec)", 0x9000 },
	{ "A000 (40960 dec)", 0xA000 },
	{ "B000 (45056 dec)", 0xB000 },
	{ "C000 (49152 dec)", 0xC000 },
	{ "D000 (53248 dec)", 0xD000 },
	{ "E000 (57344 dec)", 0xE000 },
	{ "F000 (61440 dec)", 0xF000 },
	{ nullptr, 0 },
};

const char bridge_priority_type_name[] = "bridge_priority";

const bridge_priority_p BridgeTree::bridge_priority_property (
	"Bridge Priority",
	nullptr,
	nullptr,
	static_cast<bridge_priority_p::member_getter_t>(&bridge_priority),
	static_cast<bridge_priority_p::member_setter_t>(&set_bridge_priority),
	0x8000
);

const temp_string_p BridgeTree::root_bridge_id_property
(
	"Root Bridge ID",
	nullptr,
	nullptr,
	static_cast<temp_string_p::member_getter_t>(&root_bridge_id),
	nullptr,
	std::nullopt
);
/*
static const TypedProperty<wstring> ExternalRootPathCost
(
	L"External Root Path Cost",
	static_cast<TypedProperty<wstring>::Getter>(&BridgeTree::GetExternalRootPathCost),
	nullptr
);

static const TypedProperty<wstring> RegionalRootBridgeId
(
	L"Regional Root Bridge Id",
	static_cast<TypedProperty<wstring>::Getter>(&BridgeTree::GetRegionalRootBridgeId),
	nullptr
);

static const TypedProperty<wstring> InternalRootPathCost
(
	L"Internal Root Path Cost",
	static_cast<TypedProperty<wstring>::Getter>(&BridgeTree::GetInternalRootPathCost),
	nullptr
);

static const TypedProperty<wstring> DesignatedBridgeId
(
	L"Designated Bridge Id",
	static_cast<TypedProperty<wstring>::Getter>(&BridgeTree::GetDesignatedBridgeId),
	nullptr
);

static const TypedProperty<wstring> DesignatedPortId
(
	L"Designated Port Id",
	static_cast<TypedProperty<wstring>::Getter>(&BridgeTree::GetDesignatedPortId),
	nullptr
);

static const TypedProperty<wstring> ReceivingPortId
(
	L"Receiving Port Id",
	static_cast<TypedProperty<wstring>::Getter>(&BridgeTree::GetReceivingPortId),
	nullptr
);
*/

const edge::uint32_p BridgeTree::hello_time_property {
	"HelloTime",
	&root_times_group,
	nullptr,
	static_cast<edge::uint32_p::member_getter_t>(&BridgeTree::hello_time),
	nullptr,
	std::nullopt };

const edge::uint32_p BridgeTree::max_age_property {
	"MaxAge",
	&root_times_group,
	nullptr,
	static_cast<edge::uint32_p::member_getter_t>(&BridgeTree::max_age),
	nullptr,
	std::nullopt };

const edge::uint32_p BridgeTree::forward_delay_property {
	"ForwardDelay",
	&root_times_group,
	nullptr,
	static_cast<edge::uint32_p::member_getter_t>(&BridgeTree::forward_delay),
	nullptr,
	std::nullopt };

const edge::uint32_p BridgeTree::message_age_property {
	"MessageAge",
	&root_times_group,
	nullptr,
	static_cast<edge::uint32_p::member_getter_t>(&BridgeTree::message_age),
	nullptr,
	std::nullopt };

const edge::uint32_p BridgeTree::remaining_hops_property {
	"remainingHops",
	&root_times_group,
	nullptr,
	static_cast<edge::uint32_p::member_getter_t>(&remaining_hops),
	nullptr,
	std::nullopt };

const edge::property* const BridgeTree::_properties[] =
{
	&bridge_priority_property,
	&root_bridge_id_property,
/*	&ExternalRootPathCost,
	&RegionalRootBridgeId,
	&InternalRootPathCost,
	&DesignatedBridgeId,
	&DesignatedPortId,
	&ReceivingPortId,
*/
	&hello_time_property,
	&max_age_property,
	&forward_delay_property,
	&message_age_property,
	&remaining_hops_property
};

const edge::type_t BridgeTree::_type = { "BridgeTree", &base::_type, _properties };