#include "component.h"
#include "defines.h"

#include <QRegExp>
#include <QVariantList>

Component::Component(QString name, unsigned int code, QString group, QString function, QJsonObject fields, MainWindow* window):
	name(name),
	code(code),
	group(group),
	function(function),
	fields(fields),
	window(window)
{
}

QJsonObject
Component::decode(QByteArray blob, uint offset, uint end) throw(ConvertException) {
	if(QString::compare(function, "generic")==0) {
		return decodeGeneric(blob, offset, end);
	}
	return QJsonObject();
}

QJsonObject
Component::decodeGeneric(QByteArray blob, uint offset, uint end) throw(ConvertException){
	QJsonObject comp;
	comp["type"] = name.remove(' ');
	QStringList keys = fields.keys();
	bool lastWasABitField = false;
	for(int i=0; i<keys.length(); i++) {
		if(offset >= end) {
			break;
		}
		QString k = keys[i];
		QJsonValue f = fields[k];
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
					value = QJsonValue((double)getUInt32(blob, offset));
					break;
				default:
					throw new ConvertException("Invalid field size: "+f.toString()+".");
			}
			lastWasABitField = false;
		} else if(func) {
			result = callSizeFunction(function, blob, offset);
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
			result = callSizeFunction(function, blob, offset, fArray[1]);
			value = result["value"];
			if(fArray.size()>=2) { // further processing if wanted
				value = callSizeFunction(fArray[2].toString(), blob, offset, value);
			}
			size = (uint)(result["size"].toDouble());
		}
		
		// save value or function result of value in field
		comp[k] = value;
		if(VERBOSE) log("k: "+k+", val: "+value.toString()+", offset: "+offset);
		offset += size;
	}
	return comp;
}

QJsonObject
Component::callSizeFunction(QString function, QByteArray blob, uint offset, QJsonValue other) throw(ConvertException) {
	if(function=="UInt32") {
		QJsonObject map;
		map["value"] = (double)getUInt32(blob, offset);
		map["size"] = SIZE_INT;
		return map;
	} else {
		throw new ConvertException("Method '"+function+"' was not found. (Correct capitalization?)");
	}
}

uint
Component::getUInt32(QByteArray blob, uint offset) {
	return *((uint*)(blob.constData()+offset));
}

void
Component::log(QString message) {
	this->window->log(message);
}
