#include "converter.h"
#include "defines.h"

#include <QRegExp>
#include <QVariantList>
#include <QDateTime>

Converter::Converter(Job job,
						QJsonArray* components,
						QJsonObject* tables,
						QHash<int, QByteArray>* componentDllCodes,
						MainWindow* window,
						QByteArray blob,
						uint offset):
	job(job),
	file(job.file()),
	components(components),
	tables(tables),
	componentDllCodes(componentDllCodes),
	blob(blob),
	offset(offset),
	window(window),
	indexCounter(0),
	error_(false),
	errorCount_(0),
	oldVersion(false)
{}

void
Converter::worker_toJson() {
	QString result = toJson();
	emit worker_done(this, thread(), &job, result);
}

QString
Converter::toJson() {
	if(file.exists()) {
		QFile presetFile(file.filePath());
		presetFile.open(QFile::ReadOnly);
		blob = presetFile.readAll();
		presetFile.close();
	} else {
		log("File '"+file.fileName()+"' not found.", /*error*/true);
		return "";
	}
	//log("Decoding "+file.fileName()+".");
	// now build the preset JSON document
	bool ok = true;
	QJsonDocument preset;
	QJsonObject rootObj;
	rootObj[ordKey("name")] = file.completeBaseName();
	rootObj[ordKey("date")] = file.lastModified().toString(Qt::ISODate);
	//rootObj["author"] = guessAuthor(file.fileName()); // TODO: extract likely author name from filename
	QJsonValue clearFrame = decodePresetHeader();
	offset = AVS_HEADER_LENGTH;
	if(clearFrame.isBool()) {
		rootObj[ordKey("clearFrame")] = clearFrame;
		rootObj[ordKey("components")] = decode();
		ok &= !error();
	} else {
		log("Invalid header", /*error*/true);
		ok = false;
	}
	
	preset.setObject(rootObj);
	QString json = QString(preset.toJson(window->indent() ? QJsonDocument::Indented : QJsonDocument::Compact));
	if(json.isEmpty()) {
		log("JSON output is empty.", /*error*/true);
		ok = false;
	}
	if(!ok) {
		log("Failed to convert <b>"+file.fileName()+"</b>.", /*error*/true);
		return "";
	} else {
		//log("<b>"+file.fileName()+"</b> converted.");
		return json;
	}
}

QJsonValue
Converter::decodePresetHeader() {
	QByteArray presetHeader0_1("Nullsoft AVS Preset 0.1");
	QByteArray presetHeader0_2("Nullsoft AVS Preset 0.2");
	presetHeader0_1[AVS_HEADER_LENGTH-2] = 0x1A; // there is one final '26' byte after the readable header ...
	presetHeader0_2[AVS_HEADER_LENGTH-2] = 0x1A; // ... for that repurpose the \0 byte position from the str constructor above.
	if(!blob.startsWith(presetHeader0_2)) {
		if(blob.startsWith(presetHeader0_1)) { // 0.1 only if 0.2 failed because it's far rarer.
			oldVersion = true;
		} else {
			log("Invalid preset header.", /*error*/true);
			return QJsonValue(0);
		}
	}
	return QJsonValue(blob[AVS_HEADER_LENGTH-1]==(char)1); // "Clear Every Frame"
}

QJsonArray
Converter::decode() {
	QJsonArray componentArray;
	while(offset < (uint)blob.length()) {
		uint code = uInt32(blob, offset);
		int i = getComponentIndex(code);
		bool isDll = code!=0xfffffffe && code>BUILTIN_MAX;
		uint size = Converter::uInt32(blob, offset+SIZE_INT+isDll*32);
		if(size==0) {
			log(QString("Component size at offset %1 is zero.").arg(offset), /*error*/true);
			break;
		}
		QJsonObject res;
		offset += SIZE_INT*2+isDll*32;
		if(i<0) {
			res["type"] = QJsonValue("Unknown component: ("+QString("%1").arg(-i)+")");
		} else {
			QJsonObject compConf = components->at(i).toObject();
			res = decodeComponent(offset+size,
									compConf["name"].toString(),
									compConf["group"].toString(),
									compConf["func"].toString(),
									compConf["fields"].toArray());
		}
		componentArray.append(res);
		offset += size;
	}
	return componentArray;
}

int
Converter::getComponentIndex(uint code) {
	for (int i = 0; i < components->size(); i++) {
		QJsonValue currentCode = components->at(i).toObject()["code"];
		if(currentCode.isString()) {
			if(code == (uint)(currentCode.toString().toUInt(0,0))) {
				if(VERBOSE>=1) log("Found component: "+components->at(i).toObject()["name"].toString()+" ("+QString("%1").arg(code)+")");
				return i;
			}
		} else if(currentCode.isArray()) {
			if(blob.mid(offset+SIZE_INT, 32).startsWith(componentDllCodes->value(i))) {
				if(VERBOSE>=1) log("Found component: "+components->at(i).toObject()["name"].toString());
				return i;
			}
		}
	}
	QString unknownId = QString("%1").arg(code);
	if(code>BUILTIN_MAX) {
		unknownId = sizeString(offset+SIZE_INT, 32);
	}
	log("Found unknown component (ID: "+unknownId+")", /*error*/true);
	return -code;
}

// In JS it just called the function by its string name in decode().
// We don't have that luxury here, hence this intermediate function.
QJsonObject
Converter::decodeComponent(uint end, QString name, QString group, QString function, QJsonArray fields) {
	if(function == "generic") {
		return decodeGeneric(offset, end, name, group, fields);
	} else if(function == "movement") {
		return decodeMovement(offset);
	} else if(function == "effectList") {
		return decodeEffectList(offset);
	} else if(function == "simple") {
		return decodeSimple(offset);
	} else if(function == "avi") {
		return decodeAvi(offset);
	}
	log("Decode function for '"+function+"' was not found.", /*error*/true);
	return QJsonObject();
}

QJsonObject
Converter::decodeGeneric(uint offset, uint end, QString name, QString group, QJsonArray fields) {
	QJsonObject comp;
	comp[ordKey("type")] = name.remove(' ');
	if(ALL_FIELDS) {
		comp[ordKey("group")] = group;
	}
	bool lastWasABitField = false;
	for (int i = 0; i < fields.size(); ++i) {
		if(offset >= end) {
			if(offset>end) { // newer versions of some effects might have an additional field. ignore just the one surplus, and only nag when it's more.
				log(QString("Warning: Component (\"%1\") went over its size while scanning. (offset: %2, end: %3)").arg(name).arg(offset).arg(end), /*error*/false);
			}
			break;
		}
		QString k = fields[i].toObject().keys()[0];
		QJsonValue f = fields[i].toObject()[k];
		if(VERBOSE>=2) log("Field: "+k);
		if(k.startsWith("null")) {
			offset += (uint)f.toDouble();
			// '"null": "0"' resets bitfield continuity to allow several consecutive bitfields (hasn't been necessary yet thogh)
			lastWasABitField = false;
			continue;
		}
		uint size = 0;
		QJsonValue value;
		bool number = f.isDouble();
		bool func = f.isString();
		bool array = f.isArray();
		if(number) {
			switch((uint)f.toDouble()) {
				case 1:
					size = 1;
					value = QJsonValue(blob[offset]);
					break;
				case SIZE_INT:
					size = SIZE_INT;
					value = QJsonValue((double)uInt32(blob, offset));
					break;
				default:
					log("Invalid field size: "+QString("%1").arg(f.toDouble())+".", /*error*/true);
			}
			lastWasABitField = false;
		} else if(func) {
			value = call(f.toString(), offset, &size);
			lastWasABitField = false;
		} else if(array && f.toArray().size()>=2) {
			QJsonArray fArray = f.toArray();
			if(fArray[0].toString()=="bit") {
				if(lastWasABitField) {
					offset -= 1; // compensate to stay in same bitfield
				}
				lastWasABitField = true;
			} else {
				lastWasABitField = false;
			}
			value = call(fArray[0].toString(), offset, &size, fArray[1]);
			if(fArray.size()>2) { // further processing if wanted
				value = call2nd(fArray[2].toString(), value);
			}
			if(VERBOSE>=2) log(QString("Size: %1").arg(size));
		}
		
		// save value or function result of value in field
		comp[ordKey(k)] = value;
		if(VERBOSE>=2) log("k: "+k+", val: "+value.toString()+", offset: "+QString("%1").arg(offset));
		offset += size;
	}
	return comp;
}

QJsonObject
Converter::decodeEffectList(uint offset) {
	uint size = uInt32(blob, offset-SIZE_INT);
	QJsonObject comp;
	
	comp[ordKey("type")] = QString("EffectList");
	comp[ordKey("enabled")] = bit(offset, 1)!=1;
	comp[ordKey("clearFrame")] = bit(offset, 0)==1;
	comp[ordKey("input")] = call("blendmodeIn", offset+2, NULL, 1);
	comp[ordKey("output")] = call("blendmodeOut", offset+3, NULL, 1);
	//ignore constant el config size of 36 bytes (9 x int32)
	comp[ordKey("inAdjustBlend")] = (double)uInt32(blob, offset+5);
	comp[ordKey("outAdjustBlend")] = (double)uInt32(blob, offset+9);
	comp[ordKey("inBuffer")] = (double)uInt32(blob, offset+13);
	comp[ordKey("outBuffer")] = (double)uInt32(blob, offset+17);
	comp[ordKey("inBufferInvert")] = uInt32(blob, offset+21)==1;
	comp[ordKey("outBufferInvert")] = uInt32(blob, offset+25)==1;
	comp[ordKey("enableOnBeat")] = uInt32(blob, offset+29)==1;
	comp[ordKey("onBeatFrames")] = (double)uInt32(blob, offset+33);
	
	QByteArray effectList28plusHeader = "AVS 2.8+ Effect List Config";
	uint extOffset = offset+41;
	uint contSize = size-41+SIZE_INT;
	uint contOffset = extOffset-SIZE_INT;
	if(blob.mid(extOffset, 32).startsWith(effectList28plusHeader)) {
		extOffset += 32;
		uint extSize = uInt32(blob, extOffset);
		contSize = size-41-32-SIZE_INT-extSize;
		contOffset += SIZE_INT+32+SIZE_INT+extSize;
		QJsonObject code;
		code[ordKey("enabled")] = boolean(extOffset+SIZE_INT, SIZE_INT);
		uint initSize = uInt32(blob, extOffset+SIZE_INT*2);
		code[ordKey("init")] = sizeString(extOffset+SIZE_INT*2);
		code[ordKey("frame")] = sizeString(extOffset+SIZE_INT*3+initSize);
		comp[ordKey("code")] = code;
	} //else: old Effect List format, inside components just start
	Converter content(file, components, tables, componentDllCodes, window, blob.mid(contOffset, contSize));
	connect(&content, &Converter::worker_log, window, &MainWindow::on_log);
	comp[ordKey("components")] = content.decode();
	error_ |= content.error();
	errorCount_ += content.errorCount();
	return comp;
}

QJsonObject
Converter::decodeMovement (uint offset) {
	QJsonObject comp;
	comp[ordKey("type")] = QString("Movement");
	// the special value 0 is because "old versions of AVS barf" if the id is > 15, so
	// AVS writes out 0 in that case, and sets the actual id at the end of the save block.
	uint effectIdOld = uInt32(blob, offset);
	QJsonArray effect;
	QJsonValue code;
	if(effectIdOld!=0) {
		if(effectIdOld==32767) {
			uint sizeOut = 0;
			// usually, there is a single byte reading '0x01' before custom code.
			// if it's absent it means it's an older version and the string has no size header.
			if(byte(offset+SIZE_INT)==0x01) {
				code = sizeString(offset+SIZE_INT+1, 0, &sizeOut);
			} else {
				code = ntString(offset+SIZE_INT, &sizeOut);
			}
			offset += sizeOut;
			log(QString("sizeout: %1").arg(sizeOut));
		} else {
			effect = tables->value("movementEffects").toObject()[QString("%1").arg(effectIdOld)].toArray();
		}
	} else {
		uint effectIdNew = uInt32(blob, offset+SIZE_INT*6); // 1*SIZE_INT, because of oldId=0, and 5*SIZE_INT because of the other settings.
		effect = tables->value("movementEffects").toObject()[QString("%1").arg(effectIdNew)].toArray();
	}
	if(effect.size()) {
		comp[ordKey("builtinEffect")] = effect[0].toString();
	}
	comp[ordKey("output")] = QString(uInt32(blob, offset+SIZE_INT)==1 ? "50/50" : "Replace");
	comp[ordKey("sourceMapped")] = boolean(offset+SIZE_INT*2, SIZE_INT);
	QString coordKey = ordKey("coordinates"); // need to save the key in order to be able overwrite the same value later
	comp[coordKey] = call("coordinates", offset+SIZE_INT*3, NULL, SIZE_INT);
	comp[ordKey("bilinear")] = boolean(offset+SIZE_INT*4, SIZE_INT);
	comp[ordKey("wrap")] = boolean(offset+SIZE_INT*5, SIZE_INT);
	if(effect.size() && effectIdOld!=1 && effectIdOld!=7) { // 'slight fuzzify' and 'blocky partial out' have no script representation.
		code = effect[1];
		comp[coordKey] = call("coordinates", effect[2].toDouble()); // overwrite
	}
	comp[ordKey("code")] = code;
	offset += SIZE_INT*(6+(effectIdOld==0));
	log(QString("Offset: %1").arg(offset));
	return comp;
}

QJsonObject
Converter::decodeAvi (uint offset) {
	QJsonObject comp;
	uint sizeOut = 0;
	comp[ordKey("type")] = QString("AVI");
	comp[ordKey("enabled")] = boolean(offset, SIZE_INT);
	QString str = ntString(offset+SIZE_INT*3, &sizeOut);
	comp[ordKey("file")] = str;
	comp[ordKey("speed")] = (double)uInt32(blob, offset+SIZE_INT*5+sizeOut); // 0: fastest, 1000: slowest
	uint beatAdd = uInt32(blob, offset+SIZE_INT*3+sizeOut);
	if(beatAdd==1) {
		comp[ordKey("output")] = QString("50/50");
	} else {
		QJsonObject map;
		map["0"] = QString("Replace");
		map["1"] = QString("Additive");
		map["0x100000000"] = QString("50/50");
		comp[ordKey("output")] = map8(offset+SIZE_INT, map);
	}
	comp[ordKey("onBeatAdd")] = (double)beatAdd;
	comp[ordKey("persist")] = (double)uInt32(blob, offset+SIZE_INT*4+sizeOut); // 0-32
	return comp;
}

QJsonObject
Converter::decodeSimple (uint offset) {
	QJsonObject comp;
	comp[ordKey("type")] = QString("Simple");
	uint effect = uInt32(blob, offset);
	if (effect&(1<<6)) {
		comp[ordKey("audioSource")] = QString((effect&2)!=0 ? "Waveform" : "Spectrum");
		comp[ordKey("renderType")] = QString("Dots");
	} else {
		switch (effect&3) {
			case 0: // solid analyzer
				comp[ordKey("audioSource")] = QString("Spectrum");
				comp[ordKey("renderType")] = QString("Solid");
				break;
			case 1: // line analyzer
				comp[ordKey("audioSource")] = QString("Spectrum");
				comp[ordKey("renderType")] = QString("Lines");
				break;
			case 2: // line scope
				comp[ordKey("audioSource")] = QString("Waveform");
				comp[ordKey("renderType")] = QString("Lines");
				break;
			case 3: // solid scope
				comp[ordKey("audioSource")] = QString("Waveform");
				comp[ordKey("renderType")] = QString("Solid");
				break;
		}
	}
	comp[ordKey("audioChannel")] = call2nd("audioChannel", (double)((effect>>2)&3));
	comp[ordKey("positionY")] = call2nd("positionY", (double)((effect>>4)&3));
	comp[ordKey("colors")] = colorList(offset+SIZE_INT);
	return comp;
}

QJsonValue
Converter::call(QString function, uint offset, uint* sizeOut, QJsonValue other) {
	QJsonValue value;
	if(QRegExp("code[IFBP]{3,4}(nt)?").exactMatch(function)) {
		// (Point,) Frame, Beat, Init code fields, in various arrangements - read and reorder to I,F,B(,P).
		// look up the sort map from 'tables.json', lines are 'need'-sorted with 'is'-index.
		if(VERBOSE>=2) log("Table code fields function '"+function+"'.");
		return codeSection(offset, tables->value(function).toArray(), function.endsWith("nt"), sizeOut);
	} else if(tables->contains(function)) { // one of the generic table->value lookups
		QString code = QString("%1").arg((uint)(other.toDouble()<1.001 ? blob[offset] : uInt32(blob, offset)));
		if(sizeOut) *sizeOut = (uint)other.toDouble();
		value = tables->value(function).toObject()[code];
	} else if(function=="sizeString") {
		value = sizeString(offset, (uint)other.toDouble(), sizeOut);
	} else if(function=="ntString") {
		value = ntString(offset, sizeOut);
	} else if(function=="uInt32") {
		if(sizeOut) *sizeOut = SIZE_INT;
		value = (double)uInt32(blob, offset);
	} else if(function=="int32") {
		value = (double)int32(offset, sizeOut);
	} else if(function=="float32") {
		value = float32(offset, sizeOut);
	} else if(function=="boolean") {
		value = boolean(offset, (uint)other.toDouble(), sizeOut);
	} else if(function=="bit") {
		value = bit(offset, other, sizeOut);
	} else if(function=="map1") {
		if(sizeOut) *sizeOut = 1;
		value = map1(offset, other.toObject());
	} else if(function=="map4") {
		if(sizeOut) *sizeOut = SIZE_INT;
		value = map4(offset, other.toObject());
	} else if(function=="map8") {
		if(sizeOut) *sizeOut = SIZE_INT*2;
		value = map8(offset, other.toObject());
	} else if(function=="radioButton") {
		value = radioButton(offset, other.toObject(), sizeOut);
	} else if(function=="bufferNum") {
		value = bufferNum(offset, (uint)other.toDouble(), sizeOut);
	} else if(function=="colorList") {
		value = colorList(offset, sizeOut);
	} else if(function=="colorMaps") {
		value = colorMaps(offset, sizeOut);
	} else if(function=="color") {
		value = color(offset, sizeOut);
	} else if(function=="convoFilter") {
		value = convoFilter(offset, other, sizeOut);
	} else {
		log("Unknown function '"+function+"'.", /*error*/true);
	}
	return value;
}

QJsonValue
Converter::call2nd(QString function, QJsonValue value) {
	if(tables->contains(function)) { // one of the generic table->value lookups
		QString code = QString("%1").arg((uint)value.toDouble());
		return tables->value(function).toObject()[code];
	} else if(function=="boolified") {
		return value.toDouble()!=0;
	} else if(function=="semiColSplit") {
		return semiColSplit(value.toString());
	} else {
		log("Unknown function '"+function+"' (2nd).", /*error*/true);
		return QJsonValue::Undefined;
	}
}

//// decode helpers

// generic mapping functions (in 1, 4, 8 byte flavor and radio button mode (multiple int32)) to map values to one of a set of strings
QJsonValue
Converter::map1(uint offset, QJsonObject map) {
	return mapping(map, blob[offset]);
}

QJsonValue
Converter::map4(uint offset, QJsonObject map) {
	return mapping(map, uInt32(blob, offset));
}

QJsonValue
Converter::map8(uint offset, QJsonObject map) {
	return mapping(map, uInt64(blob, offset));
}

QJsonValue
Converter::radioButton(uint offset, QJsonObject map, uint* sizeOut) {
	int key = 0;
	for (int i = 0; i < map.size(); i++) {
		bool on = uInt32(blob, offset+SIZE_INT*i)!=0;
		if(on) { // in case of (erroneous) multiple selections, the last one selected wins
			key = on*(i+1);
		}
	}
	
	if(sizeOut) *sizeOut = SIZE_INT*map.size();
	return mapping(map, key);
}

QJsonValue
Converter::mapping (QJsonObject map, int key) {
	QJsonValue value = map[(QString("%1").arg(key))];
	if (value.isUndefined()) {
		log("Map: A value for key '"+QString("%1").arg(key)+"' does not exist.", /*error*/true);
	}
	return value;
}

QJsonValue
Converter::codeSection(uint offset, QJsonArray map, bool nt, uint* sizeOut) {
	QStringList strings;
	uint totalSize = 0;
	for (int i = 0; i < map.size(); i++) {
		uint sizeOutInner;
		QString str = nt||oldVersion ? ntString(offset+totalSize, &sizeOutInner) : sizeString(offset+totalSize, 0, &sizeOutInner);
		totalSize += oldVersion ? 256 : sizeOutInner;
		strings.append(str);
	};
	QJsonObject code;
	for (int i = 0; i < map.size(); i++) {
		code[ordKey(map[i].toArray()[0].toString())] = strings[(uint)(map[i].toArray()[1].toDouble())];
	}
	
	if(sizeOut) *sizeOut = totalSize;
	return code;
}


QJsonValue
Converter::colorList(uint offset, uint* sizeOut) {
	QJsonArray colors;
	uint num = uInt32(blob, offset);
	uint size = SIZE_INT+num*SIZE_INT;
	if(num>1023) {
		log(QString("Color List - unreasonably large color count (%1).").arg(num), /*error*/true);
	} else {
		while(num>0) {
			offset += SIZE_INT;
			colors.append(color(offset));
			num--;
		}
	}
	if(sizeOut) *sizeOut = size;
	return colors;
}

QJsonValue
Converter::colorMaps(uint offset, uint* sizeOut) {
	uint mapOffset = offset+480;
	QJsonArray maps;
	uint headerSize = 60; // 4B enabled, 4B num, 4B id, 48B filestring
	for (uint i = 0; i < 8; i++) {
		bool enabled = boolean(offset+headerSize*i, SIZE_INT);
		uint num = uInt32(blob, offset+headerSize*i+SIZE_INT);
		QJsonArray map = colorMap(mapOffset, num);
		// check if it's a disabled default {0: #000000, 255: #ffffff} map, and only save it if not.
		if(!enabled && map.size()==2 &&
				map[0].toObject()["color"].toString()=="#000000" && map[0].toObject()["position"].toDouble()==0 &&
				map[1].toObject()["color"].toString()=="#ffffff" && map[1].toObject()["position"].toDouble()>254.9) {
					// skip this map
		} else {
			QJsonObject curMap;
			curMap[ordKey("index")] = (double)i;
			curMap[ordKey("enabled")] = enabled;
			if(ALL_FIELDS) {
				curMap[ordKey("id")] = (double)uInt32(blob, offset+headerSize*i+SIZE_INT*2); // id of the map - not really needed.
				curMap[ordKey("fileName")] = ntString(offset+headerSize*i+SIZE_INT*3);
			}
			curMap[ordKey("map")] = map;
			maps.append(curMap);
		}
		mapOffset += num*SIZE_INT*3;
	}
	
	if(sizeOut) *sizeOut = mapOffset-offset;
	return maps;
}

QJsonArray
Converter::colorMap (uint offset, uint num) {
	QJsonArray colorMap;
	for (uint i = 0; i < num; i++) {
		uint pos = uInt32(blob, offset);
		QJsonObject colorPin;
		// No ordKey() here because it's already sorted correctly and we need to access color and pos keys in colorMaps().
		colorPin["color"] = color(offset+SIZE_INT);
		colorPin["position"] = (double)pos;
		colorMap.append(colorPin);
		offset += SIZE_INT*3; // there's a 4byte id (presumably) following each color.
	}
	return colorMap;
}

QString
Converter::color(uint offset, uint* sizeOut) {
	// Colors in AVS are saved as (A)RGB (where A is always 0).
	// Maybe one should use an alpha channel right away and set
	// that to 0xff? For now, no 4th byte means full alpha.
	if(sizeOut) *sizeOut = SIZE_INT;
	return QString("%1").arg(uInt32(blob, offset), 0, 16).rightJustified(6, '0').prepend('#');
}

QJsonValue
Converter::convoFilter(uint offset, QJsonValue dimensions, uint* sizeOut) {
	if(!dimensions.isArray() || dimensions.toArray().size()!=2) {
		log("ConvoFilter: Size must be array with x and y dimensions in dwords.", /*error*/true);
	}
	QJsonArray dimArr = dimensions.toArray();
	uint size = dimArr[0].toDouble() * dimArr[1].toDouble();
	QJsonArray data;
	for (uint i = 0; i < size; i++, offset+=SIZE_INT) {
		data.append((double)int32(offset));
	}
	QJsonObject kernel;
	kernel[ordKey("width")] = dimArr[0];
	kernel[ordKey("height")] = dimArr[1];
	kernel[ordKey("data")] = data;
	
	if(sizeOut) *sizeOut = size*SIZE_INT;
	return kernel;
}

// 'Text' needs this
QJsonValue
Converter::semiColSplit (QString str) {
	QStringList strings = str.split(';');
	if(strings.length() == 1) {
		return QJsonValue(strings[0]);
	}
	QJsonArray retVal;
	for (int i = 0; i < strings.length(); ++i) {
		retVal.append(strings[i]);
	}
	return QJsonValue(retVal);
}

QJsonValue
Converter::bufferNum(uint offset, uint size, uint* sizeOut) {
	uint code = size==1 ? blob[offset] : uInt32(blob, offset);
	if(sizeOut) *sizeOut = size==1 ? 1 : 4;
	if(code==0) {
		return QString("Current");
	} else {
		return (double)uInt32(blob, offset);
	}
}


//// Utility functions


QString
Converter::sizeString(uint offset, uint size, uint* sizeOut) {
	uint add = 0;
	QString result;
	if(size==0) {
		size = uInt32(blob, offset);
		add = SIZE_INT;
	}
	uint end = offset+size+add;
	uint i = offset+add;
	char c = blob[i];
	while(c>0 && i<end) {
		//log(QString(c));
		result.append(c);
		c = blob[++i];
	}
	if(sizeOut) *sizeOut = size+add;
	return result;
}

QString
Converter::ntString(uint offset, uint* sizeOut) {
	QString result;
	uint i = offset;
	char c = blob[i];
	while(c>0) {
		result.append(c);
		c = blob[++i];
	}
	if(sizeOut) *sizeOut = i-offset+1;
	return result;
}

bool
Converter::boolean (uint offset, uint size, uint* sizeOut) {
	if(sizeOut) *sizeOut = size==1 ? 1 : SIZE_INT;
	uint val = size==1 ? blob[offset] : uInt32(blob, offset);
	return val!=0;
}

char
Converter::byte (uint offset, uint* sizeOut) {
	if(sizeOut) *sizeOut = 1;
	if(offset > blob.length()) {
		log(QString("Offset out of range in %1 (blobsize: %2, offset: %3) (byte)").arg(this->fileName()).arg(blob.length()).arg(offset), /*error*/true);
		return 0;
	}
	return *((char*)(blob.constData()+offset));
}

qint32
Converter::int32 (uint offset, uint* sizeOut) {
	if(sizeOut) *sizeOut = SIZE_INT;
	if(offset-1+SIZE_INT > blob.length()) {
		log(QString("Offset out of range in %1 (blobsize: %2, offset: %3) (int32)").arg(this->fileName()).arg(blob.length()).arg(offset), /*error*/true);
		return 0;
	}
	return *((qint32*)(blob.constData()+offset));
}

uint
Converter::uInt32(QByteArray blob, uint offset) {
	if(offset-1+SIZE_INT > blob.length()) {
		log(QString("Offset out of range in %1 (blobsize: %2, offset: %3) (uint32)").arg(this->fileName()).arg(blob.length()).arg(offset), /*error*/true);
		return 0;
	}
	return *((uint*)(blob.constData()+offset));
}

quint64
Converter::uInt64(QByteArray blob, uint offset) {
	if(offset-1+sizeof(qint64) > blob.length()) {
		log(QString("Offset out of range in %1 (blobsize: %2, offset: %3) (uint64)").arg(this->fileName()).arg(blob.length()).arg(offset), /*error*/true);
		return 0;
	}
	return *((quint64*)(blob.constData()+offset));
}

QJsonValue
Converter::float32 (uint offset, uint* sizeOut) {
	if(sizeOut) *sizeOut = 4;
	if(offset-1+sizeof(float) > blob.length()) {
		log(QString("Offset out of range in %1 (blobsize: %2, offset: %3) (float32)").arg(this->fileName()).arg(blob.length()).arg(offset), /*error*/true);
		return 0;
	}
	return *((float*)(blob.constData()+offset));
}

QJsonValue
Converter::bit(uint offset, QJsonValue pos, uint* sizeOut) {
	if(sizeOut) *sizeOut = 1;
	if(offset-1+SIZE_INT > blob.length()) {
		log(QString("Offset out of range in %1 (blobsize: %2, offset: %3) (bit)").arg(this->fileName()).arg(blob.length()).arg(offset), /*error*/true);
		return 0;
	}
	if(pos.isArray()) {
		QJsonArray posArr = pos.toArray();
		if(posArr.size()!=2) {
			log("Wrong Bitfield range", /*error*/true);
			return -1;
		} else {
			unsigned char pos0 = (unsigned char)posArr[0].toDouble();
			unsigned char pos1 = (unsigned char)posArr[1].toDouble();
			char mask = (2<<(pos1-pos0))-1;
			return (blob[offset]>>pos0)&mask;
		}
	} else {
		return (blob[offset]>>((unsigned char)pos.toDouble()))&1;
	}
}

// various helper functions
/**
 * @brief Components::ordKey injects ordering prefixes into the keys of
 *  QJsonObjects. They are removed again in Converter::postProcess().
 * @param key
 * @return 
 */
QString
Converter::ordKey(QString key) {
	return QString("__%1__").arg(indexCounter++, 5, 10, QLatin1Char('0'))+key;
}

void
Converter::log(QString message, bool error) {
	if(error) error_ = true;
	emit worker_log(message, error);
}

bool
Converter::error() {
	//log(QString("Error count: %1").arg(errCount), true);
	return error_;
}

int
Converter::errorCount() {
	return errorCount_;
}

QString
Converter::fileName()
{
	return file.fileName();
}
