var result = new Intl.Locale('und', { language: 'ru' }).toString();
if (result != "ru")
    throw "FAILED";
