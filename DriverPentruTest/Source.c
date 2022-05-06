#include "ntddk.h"

#define DEVICE_SEND CTL_CODE(FILE_DEVICE_UNKNOWN,0x801,METHOD_BUFFERED,FILE_WRITE_DATA)
#define DEVICE_REC CTL_CODE(FILE_DEVICE_UNKNOWN,0x802,METHOD_BUFFERED,FILE_READ_DATA)

#define MAX_LENGTH 256
#define MAX_STACK 5

UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\unDispozitiv");//dispozitivele trebuie sa sada in directorul "Device"
UNICODE_STRING SymLink = RTL_CONSTANT_STRING(L"\\??\\unDispozitivSimbolic");//e o coneventie in esneta sa punem symlink-urile in "??"
PDEVICE_OBJECT DeviceObject = NULL;//initializam device object-ul pe care il vom folosi

VOID Ciao(IN PDRIVER_OBJECT DriverObject)
{
	IoDeleteSymbolicLink(&SymLink);//inainte de a descarca curatam, mai intai legatura simbolica
	IoDeleteDevice(DeviceObject);//si apoi si device object-ul care a ramas
	KdPrint(("Am reusit sa ma descarc... \r\n"));
}

NTSTATUS DispatchRoutine(PDEVICE_OBJECT DeviceObject, PIRP irp)
{
	NTSTATUS status = STATUS_SUCCESS;//in mod normal intoarcem succes
	PIO_STACK_LOCATION irpsp = IoGetCurrentIrpStackLocation(irp);//aflam pe care nivel din irp stack ne aflam ca sa putem distinge rutina ce trebie executata
	switch (irpsp->MajorFunction)
	{
		case IRP_MJ_CREATE:
			DbgPrint("Am fost creat! \r\n");
			break;
		case IRP_MJ_CLOSE:
			DbgPrint("Am fost anihilat! \r\n");
			break;
		case IRP_MJ_READ:
			DbgPrint("Am citit! \r\n");
			break;
		case IRP_MJ_WRITE:
			DbgPrint("Am scris! \r\n");
			break;
		default:
			DbgPrint("Ceva merge prost... \r\n");
			status = STATUS_INVALID_PARAMETER;
			break;
	}
	irp->IoStatus.Information = 0;//nu citim sau scriem niciun octet
	irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(irp,IO_NO_INCREMENT);//??
	return status;
}

NTSTATUS DispatchControl(PDEVICE_OBJECT DeviceObject, PIRP irp)
{
	PIO_STACK_LOCATION irpsp = IoGetCurrentIrpStackLocation(irp);//aflam pe care nivel din irp stack ne aflam ca sa putem distinge rutina ce trebie executata
	NTSTATUS status = STATUS_SUCCESS;//in mod normaul ar trebui sa intoarcem succes
	ULONG length = 0;//initial lungimea tamponului folosit prin metoda BIO este nula
	PVOID buffer = irp->AssociatedIrp.SystemBuffer;//obtinem buffer-ul folosit prin metoda BIO
	ULONG inLength = irpsp->Parameters.DeviceIoControl.InputBufferLength;//lungimea tamponului pentru operatiile de citire
	ULONG outLength = irpsp->Parameters.DeviceIoControl.OutputBufferLength;//lungimea tamponului pentru operatiile de scriere
	static WCHAR stack[MAX_STACK][MAX_LENGTH] = {L"a",L"b",L"c",L"d",L"e"};//nu ramane asa
	static int sp = 0;
	switch (irpsp->Parameters.DeviceIoControl.IoControlCode)//facem selectia in functie de codul operatiei de I/O
	{
	case DEVICE_SEND:
		length = (wcsnlen(buffer, MAX_LENGTH) + 1) * 2;
		if (length < MAX_LENGTH)
		{
			KdPrint(("Am trimis: %ws", buffer));
			if (sp >= MAX_STACK)
				KdPrint(("Stiva e plina!"));
			else
			{
				wcsncpy(stack[sp], (PWCHAR)buffer, MAX_LENGTH);
				KdPrint(("Stiva a ajuns la elementul al %d-lea, care contine textul: %ws.", sp, stack[sp]));
				++sp;
			}
		}
		else
			KdPrint(("Sirul e prea mare!!!"));
		break;
	case DEVICE_REC:
		if (sp > 0)
		{
			--sp;
			wcscpy(buffer, stack[sp]);
			length = (wcsnlen(buffer, MAX_LENGTH) + 1) * 2;
			if (length < MAX_LENGTH)//ne asiguram ca ce trimitem se termina in NULL
				KdPrint(("Am primit al %d-lea element din stiva care contine textul: %ws.", sp,buffer));
			else
				KdPrint(("Sirul e prea mare!!!"));
		}
		else
			KdPrint(("Stiva e goala!"));
		break;
	default:
		status = STATUS_INVALID_PARAMETER;
		break;
	}
	irp->IoStatus.Status = status;//din nou completam campurile lui IoStatus
	irp->IoStatus.Information = length;
	IoCompleteRequest(irp,IO_NO_INCREMENT);//nu mai avem nimic de facut
	return status;//intoarcem codul de succes sau de eroare
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
	DriverObject->DriverUnload = Ciao;//numele pointer-ului catre rutina de executat la unload
	NTSTATUS status = IoCreateDevice(DriverObject, 0L,&DeviceName,FILE_DEVICE_UNKNOWN,FILE_DEVICE_SECURE_OPEN,FALSE,&DeviceObject);//cream un device object
	if (!NT_SUCCESS(status))//daca nu se creeaza device object-ul
	{
		KdPrint(("Nu a putut fi creat dispozitivul! Nashpa! \r\n"));//se afiseaza mesaj de eorare doar in debug mode
		return status;
	}
	status = IoCreateSymbolicLink(&SymLink,&DeviceName);//user apps nu pot lucra cu dispozitivul propriu-zis, ci doar cu legaturi simbolice
	if (!NT_SUCCESS(status))//din nou...verificam daca a fost creata legatura simbolica
	{
		KdPrint(("Nu s-a putut crea legatura simbolica! Si mai nashpa, ca trebuie stearsa! \r\n"));
		IoDeleteDevice(DeviceObject);//inainte de a incheia nu trebuie sa lasam deseurile sa zburde, curatam obiectul creat degeaba!!!
		return status;
	}
	KdPrint(("Am reusit sa ma incarc! \r\n"));
	int i;
	for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; ++i)
	{
		//rutina executata in cazul apelului vreunei rutine majore din driver (cu indexul i) este:
		DriverObject->MajorFunction[i] = DispatchRoutine;
	}
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchControl;
	return STATUS_SUCCESS;
}