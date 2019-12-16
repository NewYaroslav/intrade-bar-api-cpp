## Округление цены

```JavaScript
function round(price, type){
	if(type == "jpy"){
		let accur = 4;
		let price_with_accur = price.toFixed(accur);
		if (price_with_accur.slice(-1) == "5"){
			return (Math.ceil(price_with_accur * 1000) / 1000);
		} else {
			return (Math.round(price_with_accur * 1000) / 1000);
		}
	} else {
		let accur = 6;
		let price_with_accur = price.toFixed(accur);
		if (price_with_accur.slice(-1) == "5"){
			return (Math.ceil(price_with_accur * 100000) / 100000);
		} else {
			return (Math.round(price_with_accur * 100000) / 100000);
		}
	}
}
```

## Получение котировок через https

https://intrade.bar/dataWs.txt

## Получение теней баров через https

https://intrade.bar/shadows.json

## Получение времени сервера

https://intrade.bar/time



