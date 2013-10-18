#include "components.h"
#include "defines.h"

#include <QRegExp>
#include <QVariantList>

Components::Components(QJsonArray* components,
					   QJsonObject* tables,
					   QHash<int, QByteArray>* componentDllCodes,
					   QByteArray blob,
					   uint offset,
					   MainWindow* window):
	components(components),
	tables(tables),
	componentDllCodes(componentDllCodes),
	blob(blob),
	offset(offset),
	window(window)
{}

QJsonArray
Components::decode() {
	QJsonArray componentArray;
	while(offset < (uint)blob.length()) {
		uint code = Components::uInt32(blob, offset);
		int i = getComponentIndex(code, blob, offset);
		bool isDll = code!=0xfffffffe && code>BUILTIN_MAX;
		uint size = Components::uInt32(blob, offset+SIZE_INT+isDll*32);
		QJsonObject res;
		if(i<0) {
			res["type"] = QJsonValue("Unknown: ("+QString("%1").arg(-i)+")");
		} else {
			offset += SIZE_INT*2+isDll*32;
			QJsonObject compConf = components->at(i).toObject();
			res = decodeComponent(offset+size,
								  compConf["name"].toString(),
								  compConf["group"].toString(),
								  compConf["func"].toString(),
								  compConf["fields"].toArray());
		}
		componentArray.append(res);
		offset += size + SIZE_INT*2 + isDll*32;
	}
	return componentArray;
}

int
Components::getComponentIndex(uint code, QByteArray blob, uint offset) {
	for (int i = 0; i < components->size(); i++) {
		QJsonValue currentCode = components->at(i).toObject()["code"];
		if(currentCode.isString()) {
			if(code == (uint)(currentCode.toString().toUInt(0,0))) {
				window->log("Found component: "+components->at(i).toObject()["name"].toString()+" ("+QString("%1").arg(code)+")");
				return i;
			}
		} else if(currentCode.isArray()) {
			if(blob.mid(offset+SIZE_INT, 32).startsWith(componentDllCodes->value(i))) {
				window->log("Found component: "+components->at(i).toObject()["name"].toString());
				return i;
			}
		}
	}
	window->log("Found unknown component (code: "+QString("%1").arg(code)+")", /*error*/true);
	return -code;
}

// In JS it just called the function by its string name in decode().
// We don't have that luxury here, hence this intermediate function.
QJsonObject
Components::decodeComponent(uint end, QString name, QString group, QString function, QJsonArray fields) {
	if(function == "generic") {
		return decodeGeneric(end, name, group, fields);
	} else if(function == "movement") {
		return decodeMovement();
	} else if(function == "effectList") {
		return decodeEffectList();
	} else if(function == "simple") {
		return decodeSimple();
	} else if(function == "avi") {
		return decodeAvi();
	}
	log("Decode function for '"+function+"' was not found.");
	return QJsonObject();
}

QJsonObject
Components::decodeGeneric(uint end, QString name, QString group, QJsonArray fields) {
	QJsonObject comp;
	comp["type"] = name.remove(' ');
	bool lastWasABitField = false;
	for (int i = 0; i < fields.size(); ++i) {
		if(offset >= end) {
			log("Warning: Component went over its size while scanning.");
			break;
		}
		QString k = fields[i].toObject().keys()[0];
		QJsonValue f = fields[i].toObject()[k];
		log("Field: "+k);
		if(QRegExp("^null[_0-9]*$").exactMatch(k)) {
			offset += (uint)f.toDouble();
			// '"null_": "0"' resets bitfield continuity to allow several consecutive bitfields (hasn't been necessary yet thogh)
			lastWasABitField = false;
			continue;
		}
		uint size = 0;
		QJsonObject result;
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
					log("Invalid field size: "+f.toString()+".", /*error*/true);
			}
			lastWasABitField = false;
		} else if(func) {
			result = callSizeFunction(f.toString(), offset);
			if(result.size()!=2) break;
			value = result["value"];
			size = (uint)(result["size"].toDouble());
			lastWasABitField = false;
		} else if(array && f.toArray().size()>=2) {
			QJsonArray fArray = f.toArray();
			if(fArray[0].toString()=="Bit") {
				if(lastWasABitField) {
					offset -= 1; // compensate to stay in same bitfield
				}
				lastWasABitField = true;
			} else {
				lastWasABitField = false;
			}
			result = callSizeFunction(fArray[0].toString(), offset, fArray[1]);
			if(result.size()!=2) break;
			value = result["value"];
			if(fArray.size()>2) { // further processing if wanted
				value = call2ndFunction(fArray[2].toString(), offset, value);
				if(result.size()!=2) break;
			}
			log(QString("Size: %1").arg(result["size"].toDouble()));
			size = (uint)(result["size"].toDouble());
		}
		
		// save value or function result of value in field
		comp[k] = value;
		if(VERBOSE) log("k: "+k+", val: "+value.toString()+", offset: "+QString("%1").arg(offset));
		offset += size;
	}
	return comp;
}

QJsonObject
Components::decodeMovement() {
	return QJsonObject();
}

QJsonObject
Components::decodeEffectList() {
	return QJsonObject();
}

QJsonObject
Components::decodeSimple() {
	return QJsonObject();
}

QJsonObject
Components::decodeAvi() {
	return QJsonObject();
}

QJsonObject
Components::callSizeFunction(QString function, uint offset, QJsonValue other) {
	QJsonObject tuple;
	if(QRegExp("code[IFBP]{3,4}(nt)?").exactMatch(function)) {
		// (Point,) Frame, Beat, Init code fields, in various arrangements - read and reorder to I,F,B(,P).
		// look up the sort map from 'tables.json', lines are 'need'-sorted with 'is'-index.
		log("Table code fields function '"+function+"'.");
		return codeSection(offset, tables->value(function).toArray(), function.endsWith("nt"));
	} else if(tables->contains(function)) { // one of the generic table->value lookups
		QString code = QString("%1").arg((uint)(other.toDouble()<1.001 ? blob[offset] : uInt32(blob, offset)));
		tuple["value"] = tables->value(function).toObject()[code];
		tuple["size"] = other.toDouble();
	} else if(function=="sizeString") {
		QJsonObject result = sizeString(offset, (uint)other.toDouble());
		tuple["value"] = result["value"].toString();
		tuple["size"] = result["size"].toDouble();
	} else if(function=="ntString") {
		QJsonObject result = ntString(offset);
		tuple["value"] = result["value"].toString();
		tuple["size"] = result["size"].toDouble();
	} else if(function=="int32") {
		tuple["value"] = int32(offset)["value"].toDouble();
		tuple["size"] = SIZE_INT;
	} else if(function=="map1") {
		tuple["value"] = map1(offset, other.toObject())["value"].toString();
		tuple["size"] = 1;
	} else if(function=="map4") {
		tuple["value"] = map4(offset, other.toObject())["value"].toString();
		tuple["size"] = SIZE_INT;
	} else if(function=="map8") {
		tuple["value"] = map8(offset, other.toObject())["value"].toString();
		tuple["size"] = SIZE_INT*2;
	} else if(function=="radioButton") {
		QJsonObject result = radioButton(offset, other.toObject());
		tuple["value"] = result["value"].toString();
		tuple["size"] = result["size"].toDouble();
	} else if(function=="bufferNum") {
		tuple["value"] = bufferNum(offset, (uint)other.toDouble())["value"].toDouble();
		tuple["size"] = other.toDouble();
	} else if(function=="colorList") {
		QJsonObject result = colorList(offset);
		tuple["value"] = result["value"].toArray();
		tuple["size"] = result["size"].toDouble();
	} else if(function=="colorMaps") {
		QJsonObject result = colorMaps(offset);
		tuple["value"] = result["value"].toArray();
		tuple["size"] = result["size"].toDouble();
	} else if(function=="convoFilter") {
		QJsonObject result = convoFilter(offset, other);
		tuple["value"] = result["value"].toObject();
		tuple["size"] = result["size"].toDouble();
	} else if(function=="boolean") {
		tuple["value"] = boolean(offset, (uint)other.toDouble())["value"].toBool();
		tuple["size"] = other.toDouble();
	} else if(function=="bit") {
		tuple["value"] = bit(offset, other)["value"].toDouble();
		tuple["size"] = 1;
	} else {
		log("Unknown function '"+function+"'.", /*error*/true);
	}
	qDebug() << "Function: '" << function << "', result: '" << tuple["value"] << "'.";
	return tuple;
}

QJsonValue
Components::call2ndFunction(QString function, uint offset, QJsonValue value) {
	QJsonValue retVal = QJsonValue::Undefined;
	if(tables->contains(function)) { // one of the generic table->value lookups
		QString code = QString("%1").arg((uint)(value.toDouble()<1.001 ? blob[offset] : uInt32(blob, offset)));
		return tables->value(function).toObject()[code];
	} else if(function=="boolified") {
		return value!=0;
	} else {
		log("Unknown function '"+function+"'.", /*error*/true);
		return retVal;
	}
}

//// decode helpers

// generic mapping functions (in 1, 4, 8 byte flavor and radio button mode (multiple int32)) to map values to one of a set of strings
QJsonObject
Components::map1 (uint offset, QJsonObject map) {
	QJsonObject retVal;
	retVal["value"] = mapping(map, blob[offset]);
	retVal["size"] = 1;
	return retVal;
}

QJsonObject
Components::map4 (uint offset, QJsonObject map) {
	QJsonObject retVal;
	retVal["value"] = mapping(map, uInt32(blob, offset));
	retVal["size"] = SIZE_INT;
	return retVal;
}

QJsonObject
Components::map8 (uint offset, QJsonObject map) {
	QJsonObject retVal;
	retVal["value"] = mapping(map, uInt64(blob, offset));
	retVal["size"] = SIZE_INT*2;
	return retVal;
}

QJsonObject
Components::radioButton (uint offset, QJsonObject map) {
	int key = 0;
	for (int i = 0; i < map.size(); i++) {
		bool on = uInt32(blob, offset+SIZE_INT*i)!=0;
		if(on) { // in case of (erroneous) multiple selections, the last one selected wins
			key = on*(i+1);
		}
	}
	
	QJsonObject retVal;
	retVal["value"] = mapping(map, key);
	retVal["size"] =  SIZE_INT*map.size();
	return retVal;
}

QJsonValue
Components::mapping (QJsonObject map, int key) {
	QJsonValue value = map[(QString("%1").arg(key))];
	if (value.isUndefined()) {
		log("Map: A value for key '"+QString("%1").arg(key)+"' does not exist.", /*error*/true);
	}
	return value;
}

QJsonObject
Components::codeSection (uint offset, QJsonArray map, bool nt) {
	QStringList strings;
	uint totalSize = 0;
	for (int i = 0; i < map.size(); i++) {
		QJsonObject strAndSize = nt ? ntString(offset+totalSize) : sizeString(offset+totalSize);
		totalSize += (uint)(strAndSize["size"].toDouble());
		strings.append(strAndSize["value"].toString());
	};
	QJsonObject code;
	for (int i = 0; i < map.size(); i++) {
		code[map[i].toArray()[0].toString()] = strings[(uint)(map[i].toArray()[1].toDouble())];
	}
	
	QJsonObject retVal;
	retVal["value"] = code;
	retVal["size"] = (double)totalSize;
	return retVal;
}


QJsonObject
Components::colorList (uint offset) {
	QJsonArray colors;
	uint num = uInt32(blob, offset);
	uint size = SIZE_INT+num*SIZE_INT;
	while(num>0) {
		offset += SIZE_INT;
		colors.append(color(offset)["value"]);
		num--;
	}
	
	QJsonObject retVal;
	retVal["value"] = colors;
	retVal["size"] = (double)size;
	return retVal;
}

QJsonObject
Components::colorMaps (uint offset) {
	uint mapOffset = offset+480;
	QJsonArray maps;
	uint headerSize = 60; // 4B enabled, 4B num, 4B id, 48B filestring
	for (uint i = 0; i < 8; i++) {
		bool enabled = boolean(offset+headerSize*i, SIZE_INT)["value"].toBool();
		uint num = uInt32(blob, offset+headerSize*i+SIZE_INT);
		QJsonArray map = colorMap(mapOffset, num);
		// check if it's a disabled default {0: #000000, 255: #ffffff} map, and only save it if not.
		if(!enabled && map.size()==2 && 
				map[0].toObject()["color"].toString()=="#000000" && map[0].toObject()["position"].toDouble()==0 &&
				map[1].toObject()["color"].toString()=="#ffffff" && map[1].toObject()["position"].toDouble()>254.9) {
			// skip this map
		} else {
			QJsonObject curMap;
			curMap["index"] = (double)i;
			curMap["enabled"] = enabled;
			if(ALL_FIELDS) {
				uint id = uInt32(blob, offset+headerSize*i+SIZE_INT*2); // id of the map - not really needed.
				QString mapFile = ntString(offset+headerSize*i+SIZE_INT*3)["value"].toString();
				curMap["id"] = (double)id;
				curMap["fileName"] = mapFile;
			}
			curMap["map"] = map;
			maps.append(curMap);
		}
		mapOffset += num*SIZE_INT*3;
	}
	
	QJsonObject retVal;
	retVal["value"] = maps;
	retVal["size"] = (double)(mapOffset-offset);
	return retVal;
}

QJsonArray
Components::colorMap (uint offset, uint num) {
	QJsonArray colorMap;
	for (uint i = 0; i < num; i++) {
		uint pos = uInt32(blob, offset);
		QString col = color(offset+SIZE_INT)["value"].toString();
		QJsonObject colorPin;
		colorPin["color"] = col;
		colorPin["position"] = (double)pos;
		colorMap.append(colorPin);
		offset += SIZE_INT*3; // there's a 4byte id (presumably) following each color.
	}
	return colorMap;
}

QJsonObject
Components::color (uint offset) {
	// Colors in AVS are saved as (A)RGB (where A is always 0).
	// Maybe one should use an alpha channel right away and set
	// that to 0xff? For now, no 4th byte means full alpha.
	QString color = QString("%1").arg(uInt32(blob, offset), 6, 16);
	QJsonObject retVal;
	retVal["value"] = color.prepend('#');
	retVal["size"] = SIZE_INT;
	return retVal;
}

QJsonObject
Components::convoFilter (uint offset, QJsonValue dimensions) {
	if(!dimensions.isArray() || dimensions.toArray().size()!=2) {
		log("ConvoFilter: Size must be array with x and y dimensions in dwords.", /*error*/true);
	}
	QJsonArray dimArr = dimensions.toArray();
	uint size = dimArr[0].toDouble() * dimArr[1].toDouble();
	QJsonArray data;
	for (uint i = 0; i < size; i++, offset+=SIZE_INT) {
		data.append((double)(int32(offset)["value"].toDouble()));
	}
	QJsonObject kernel;
	kernel["width"] = dimArr[0];
	kernel["height"] = dimArr[1];
	kernel["data"] = data;
	
	QJsonObject retVal;
	retVal["value"] = kernel;
	retVal["size"] = (double)size*SIZE_INT;
	return retVal;
}

// 'Text' needs this
QJsonValue
Components::semiColSplit (QString str) {
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

QJsonObject
Components::bufferNum (uint offset, uint size) {
	uint code = size==1 ? blob[offset] : uInt32(blob, offset);
	QJsonObject retVal;
	if(code==0) {
		retVal["value"] = QString("Current");
	} else {
		retVal["value"] = (double)uInt32(blob, offset);
	}
	retVal["size"] = (double)size;
	return retVal;
}


//// Utility functions


QJsonObject
Components::sizeString (uint offset, uint size) {
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
		result.append(c);
		c = blob[++i];
	}
	QJsonObject retVal;
	retVal["value"] = result;
	retVal["size"] = (double)(size+add);
	return retVal;
}

QJsonObject
Components::ntString (uint offset) {
	QString result;
	uint i = offset;
	char c = blob[i];
	while(c>0) {
		result.append(c);
		c = blob[++i];
	}
	QJsonObject retVal;
	retVal["value"] = result;
	retVal["size"] = (double)(i-offset+1);
	return retVal;
}

QJsonObject
Components::boolean (uint offset, uint size) {
	uint val = size==1?blob[offset]:uInt32(blob, offset);
	QJsonObject retVal;
	retVal["value"] = val!=0;
	retVal["size"] = (double)size;
	return retVal;
}

QJsonObject
Components::int32 (uint offset) {
	qint32 value = *((qint32*)(blob.constData()+offset));
	QJsonObject retVal;
	retVal["value"] = value;
	retVal["size"] = SIZE_INT;
	return retVal;
}

QJsonObject
Components::float32 (uint offset) {
	float value = *((float*)(blob.constData()+offset));
	QJsonObject retVal;
	retVal["value"] = value;
	retVal["size"] = SIZE_INT;
	return retVal;
}

QJsonObject
Components::bit (uint offset, QJsonValue pos) {
	QJsonObject retVal;
	if(pos.isArray()) {
		QJsonArray posArr = pos.toArray();
		if(posArr.size()!=2) {
			log("Wrong Bitfield range", /*error*/true);
			retVal["value"] = -1;
		} else {
			unsigned char pos0 = (unsigned char)posArr[0].toDouble();
			unsigned char pos1 = (unsigned char)posArr[1].toDouble();
			char mask = (2<<(pos1-pos0))-1;
			retVal["value"] = (blob[offset]>>pos0)&mask;
		}
	} else {
		retVal["value"] = (blob[offset]>>((unsigned char)pos.toDouble()))&1;
	}
	retVal["size"] = 1;
	return retVal;
}

// these are static because they are needed from the outside
uint
Components::uInt32(QByteArray blob, uint offset) {
	return *((uint*)(blob.constData()+offset));
}

quint64
Components::uInt64(QByteArray blob, uint offset) {
	return *((quint64*)(blob.constData()+offset));
}


// log() wrapper, bit unnecessary, but hey...
void
Components::log(QString message, bool error) {
	window->log(message, error);
}
