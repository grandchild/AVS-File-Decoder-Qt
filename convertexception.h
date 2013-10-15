#ifndef CONVERTEXCEPTION_H
#define CONVERTEXCEPTION_H

#include <exception>
#include <QString>

class ConvertException : public std::exception
{
private:
    const char* message;
public:
    ConvertException(const char* message) : message(message) {}
    ConvertException(QString message) {
        QByteArray ba = message.toLatin1();
        this->message = ba.data();
    }
    const char* what() const throw() {
        return message;
    }
	QString qwhat() const throw() {
		return QString(message);
	}
};

#endif // CONVERTEXCEPTION_H
