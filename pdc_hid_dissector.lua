local usb_ep_dir = Field.new("usb.endpoint_address.direction")

local pdc_proto = Proto("pdc_hid", "PDC002 HID protocol")

local pdc_magic = ProtoField.uint16("pdc_hid.magic", "Magic", base.HEX)
local pdc_timestamp = ProtoField.bytes("pdc_hid.timestamp", "Timestamp", base.SPACE)
local pdc_payload_type = ProtoField.uint8("pdc_hid.payload_type", "Payload type", base.HEX)
local pdc_payload_len = ProtoField.uint8("pdc_hid.payload_len", "Payload length", base.HEX)
local pdc_payload = ProtoField.bytes("pdc_hid.payload", "Payload", base.SPACE)
local pdc_payload_checksum = ProtoField.uint8("pdc_hid.payload_checksum", "Payload checksum", base.HEX)
local pdc_checksum = ProtoField.uint8("pdc_hid.checksum", "Checksum", base.HEX)

local pdc_erase_block = ProtoField.uint8("pdc_hid.erase_block", "Erase block", base.HEX)

local pdc_prog_address = ProtoField.uint32("pdc_hid.prog_address", "Programming address", base.HEX)
local pdc_prog_len = ProtoField.uint8("pdc_hid.prog_len", "Data length", base.HEX)
local pdc_prog_data = ProtoField.bytes("pdc_hid.prog_data", "Data", base.SPACE)

local pdc_read_address = ProtoField.uint32("pdc_hid.read_address", "Read address", base.HEX)
local pdc_read_len = ProtoField.uint16("pdc_hid.read_len", "Read length", base.HEX)
local pdc_read_data = ProtoField.bytes("pdc_hid.read_data", "Read data", base.SPACE)

pdc_proto.fields = {
	pdc_magic,
	pdc_timestamp,
	pdc_payload_type,
	pdc_payload_len,
	pdc_payload,
	pdc_payload_checksum,
	pdc_checksum,

	pdc_erase_block,

	pdc_prog_address,
	pdc_prog_len,
	pdc_prog_data,

	pdc_read_address,
	pdc_read_len,
	pdc_read_data
}

local pdc_payload_len_pe = ProtoExpert.new("pdc_hid.payload_len_err", "Invalid payload length", expert.group.MALFORMED, expert.severity.ERROR)
local pdc_payload_checksum_pe = ProtoExpert.new("pdc_hid.payload_checksum_err", "Payload checksum mismatch", expert.group.CHECKSUM, expert.severity.WARN)
local pdc_checksum_pe = ProtoExpert.new("pdc_hid.checksum_err", "Checksum mismatch", expert.group.CHECKSUM, expert.severity.WARN)

pdc_proto.experts = {
	pdc_payload_len_pe,
	pdc_payload_checksum_pe,
	pdc_checksum_pe
}

function checksum_of(data)
	local checksum = 0

	for i = 0,data:len()-1 do
		checksum = checksum + data(i, 1):uint()
	end

	return bit.band(checksum, 0xff)
end

function get_payload_type_string(payload_type, is_out)
	if payload_type == 0x01 then
		return "STATUS_ERROR"
	elseif payload_type == 0x02 then
		return "STATUS_SUCCESS"
	elseif payload_type == 0x03 then
		return "STATUS_REQUEST"
	elseif payload_type == 0x04 then
		return "FLASH_LOCK"
	elseif payload_type == 0x05 then
		return "FLASH_UNLOCK"
	elseif payload_type == 0x08 then
		return "ERASE"
	elseif payload_type == 0x09 then
		return "PROG"
	elseif payload_type == 0x0a then
		if is_out then
			return "READ_SMALL REQUEST"
		else
			return "READ_SMALL REPLY"
		end
	elseif payload_type == 0x0b then
		if is_out then
			return "READ_BIG REQUEST"
		else
			return "READ_BIG REPLY"
		end
	elseif payload_type == 0x17 then
		return "RESET"
	else
		return nil
	end
end

function pdc_proto.dissector(buffer, pinfo, tree)
	local is_out = tostring(usb_ep_dir()) == "0"

	local subtree = tree:add(pdc_proto, buffer(), "PDC002 HID Protocol")
	local raw_st = subtree:add(pdc_proto, buffer(), "Raw Packet")
	local payload_st = subtree:add(pdc_proto, buffer(), "Payload")

	raw_st:add(pdc_magic, buffer(0, 2))
	raw_st:add(pdc_timestamp, buffer(2, 6))

	local payload_type = buffer(8, 1):uint()
	local payload_type_st = raw_st:add(pdc_payload_type, buffer(8, 1))
	local payload_type_string = get_payload_type_string(payload_type, is_out)
	if payload_type_string then
		payload_type_st:append_text(" (" .. payload_type_string .. ")")
	end

	local payload_len = buffer(9, 1):uint()
	local payload_len_st = raw_st:add(pdc_payload_len, buffer(9, 1))
	if payload_len > 52 then
		payload_len_st:add_proto_expert_info(pdc_payload_len_pe)
		return
	end

	local payload = buffer(10, payload_len)
	raw_st:add(pdc_payload, buffer(10, payload_len))

	local payload_checksum_st = raw_st:add(pdc_payload_checksum, buffer(62, 1))
	if checksum_of(buffer(8, 54)) ~= buffer(62, 1):uint() then
		payload_checksum_st:add_proto_expert_info(pdc_payload_checksum_pe)
	end

	local checksum_st = raw_st:add(pdc_checksum, buffer(63, 1))
	if checksum_of(buffer(0, 62)) ~= buffer(63, 1):uint() then
		checksum_st:add_proto_expert_info(pdc_checksum_pe)
	end

	if payload_type == 0x08 then
		payload_st:add(pdc_erase_block, payload(1, 1))
	elseif payload_type == 0x09 then
		payload_st:add_le(pdc_prog_address, payload(0, 4))

		local prog_len = payload(4, 1):uint()
		payload_st:add(pdc_prog_len, payload(4, 1))

		payload_st:add(pdc_prog_data, payload(5, prog_len))
	elseif payload_type == 0x0a then
		if is_out then
			payload_st:add_le(pdc_read_address, payload(0, 4))
			payload_st:add_le(pdc_read_len, payload(4, 1))
		else
			payload_st:add(pdc_read_data, payload)
		end
	elseif payload_type == 0x0b then
		if is_out then
			payload_st:add_le(pdc_read_address, payload(0, 4))
			payload_st:add_le(pdc_read_len, payload(4, 2))
		else
			payload_st:add(pdc_read_data, payload)
		end
	end
end

-- TODO: is this the right value for the first parameter?
DissectorTable.get("usb.interrupt"):add(0x03, pdc_proto)
