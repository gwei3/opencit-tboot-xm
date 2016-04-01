#include "trustedboot.h"
#include "tpmapi.h"

extern char cH2[MAX_HASH_LEN];
extern char hashType[10]; //SHA1 or SHA256
extern char fs_root_path[1024] ;
extern unsigned char cH[MAX_HASH_LEN];

void doMeasurement() {
	static int ma_status = 0;
	if (ma_status == 1) {
		return;
	}
	DbgPrint("doMeasurement function called");
	// Do not try to perform any file operations at higher IRQL levels.
	// Instead, you may use a work item or a system worker thread to perform file operations.

	if (KeGetCurrentIrql() != PASSIVE_LEVEL)
	{
		DbgPrint("File operations not possible at PASSIVE Level");
		return;
	}

	HANDLE				handle, handle1;
	NTSTATUS			ntstatus;
	ANSI_STRING			ntName;
	UNICODE_STRING		uniName;
	IO_STATUS_BLOCK		ioStatusBlock, ioStatusBlock1;
	OBJECT_ATTRIBUTES	objAttr;

	char	hash_file[256];
	char    buffer[2] = { '\0' };
	char	calc_hash[MAX_HASH_LEN] = { '\0' };

	RtlInitUnicodeString(&uniName, L"\\DosDevices\\C:\\manifest.xml");
	InitializeObjectAttributes(&objAttr, &uniName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL, NULL);
	ntstatus = ZwCreateFile(&handle,
		GENERIC_READ,
		&objAttr, &ioStatusBlock,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		0,
		FILE_OPEN,
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL, 0);

	if (!NT_SUCCESS(ntstatus)) {
		DbgPrint("File Not Found - %s\n", "\\DosDevices\\C:\\manifest.xml");
		return;
	}

	RtlInitUnicodeString(&uniName, L"\\DosDevices\\C:\\Windows\\Logs\\MeasuredBoot\\measurement.xml");
	InitializeObjectAttributes(&objAttr, &uniName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL, NULL);

	ntstatus = ZwCreateFile(&handle1,
		GENERIC_WRITE,
		&objAttr, &ioStatusBlock1,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		0,
		FILE_OVERWRITE_IF,
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL, 0);

	if (!NT_SUCCESS(ntstatus)) {
		DbgPrint("File Not Found - %s\n", "\\DosDevices\\C:\\Windows\\Logs\\MeasuredBoot\\measurement.xml");
		ZwClose(handle);
		return;
	}

	do {
		int i = 0;
		int bytes_malloced = 64;
		char * line = (char *)malloc(bytes_malloced);
		while (TRUE)
		{
			if (i == bytes_malloced - 1)
			{
				int copy_bytes = bytes_malloced;
				bytes_malloced += bytes_malloced;
				char * new_line = (char *)malloc(bytes_malloced);;
				RtlCopyMemory(new_line, line, copy_bytes);
				line = new_line;
			}

			ntstatus = ZwReadFile(handle, NULL, NULL, NULL, &ioStatusBlock,	buffer, 1, NULL, NULL);

			if (NT_SUCCESS(ntstatus) || ntstatus != STATUS_END_OF_FILE)
			{
				line[i] = buffer[0];
				i++;
				if (buffer[0] == '\n')
				{
					DbgPrint("Line break found");
					line[i] = '\0';
					DbgPrint("%s\n", line);
					char * updated_line = NULL;
					if (updated_line = strstr(line, "<Manifest"))
					{
						struct ManifestHeader * header = ((struct ManifestHeader *)malloc(sizeof(struct ManifestHeader)));
						enum TagType tag = Manifest;
						PopulateElementAttribues(&header, tag, updated_line);
						RtlStringCbCopyA(hashType, sizeof(hashType), header->DigestAlg);
						DbgPrint("Digest Algorithm to be used : %s\n", hashType);
						WriteMeasurementFile(line, NULL, handle1, ioStatusBlock1, tag);

						free(header->DigestAlg);
						free(header);
					}
					else if (updated_line = strstr(line, "<File"))
					{
						struct ManifestFile * file = ((struct ManifestFile *)malloc(sizeof(struct ManifestFile)));
						enum TagType tag = File;
						PopulateElementAttribues(&file, tag, updated_line);
								
						DbgPrint("File Path : %s\n", file->Path);
						char *temp_ptr = calculate(file->Path, calc_hash);
						if (temp_ptr != NULL) {
							DbgPrint("temp_ptr : %s\n", temp_ptr);
							DbgPrint("calc_hash : %s\n", calc_hash);
						}
						WriteMeasurementFile(line, calc_hash, handle1, ioStatusBlock1, tag);

						free(file->Path);
						free(file);
					}
					else if (updated_line = strstr(line, "<Dir"))
					{
						struct ManifestDirectory * dir = ((struct ManifestDirectory *)malloc(sizeof(struct ManifestDirectory)));
						enum TagType tag = Directory;
						PopulateElementAttribues(&dir, tag, updated_line);
						DbgPrint("Dir Path : %s\n", dir->Path);
						
						BCRYPT_ALG_HANDLE       handle_Alg = NULL;
						BCRYPT_HASH_HANDLE      handle_Hash_object = NULL;
						NTSTATUS                status = STATUS_UNSUCCESSFUL;
						DWORD                   hash_size = 0, hashObject_size = 0;
						PBYTE                   hashObject_ptr = NULL, hash_ptr = NULL;

						char *files_buffer = (char *)malloc(MAX_LEN);
						RtlZeroMemory(files_buffer, MAX_LEN);

						status = setup_CNG_api_args(&handle_Alg, &handle_Hash_object, &hashObject_ptr, &hashObject_size, &hash_ptr, &hash_size);
						if (!NT_SUCCESS(status)) {
							DbgPrint("Could not inititalize CNG args Provider : 0x%x", status);
							goto free_dir;
						}

						if (*(dir->Include) == 0) {
							ListDirectory(dir->Path, "*", dir->Exclude, dir->Recursive, files_buffer, &handle_Hash_object);
						}
						else {
							ListDirectory(dir->Path, dir->Include, dir->Exclude, dir->Recursive, files_buffer, &handle_Hash_object);
						}

						//Dump the hash in variable and finish the Hash Object handle
						status = BCryptHashData(handle_Hash_object, files_buffer, strlen(files_buffer), 0);
						if (!NT_SUCCESS(status)) {
							DbgPrint("Could not calculate directory hash : 0x%x\n", status);
							cleanup_CNG_api_args(&handle_Alg, &handle_Hash_object, &hashObject_ptr, &hash_ptr);
							goto free_dir;
						}
						status = BCryptFinishHash(handle_Hash_object, hash_ptr, hash_size, 0);
						DbgPrint("Calculated Hash Bin : %s\n", hash_ptr);
						bin2hex(hash_ptr, hash_size, calc_hash, MAX_HASH_LEN);
						DbgPrint("Calculated Hash Hex : %s\n", calc_hash);
						generate_cumulative_hash(hash_ptr);
						cleanup_CNG_api_args(&handle_Alg, &handle_Hash_object, &hashObject_ptr, &hash_ptr);

						WriteMeasurementFile(line, calc_hash, handle1, ioStatusBlock1, tag);

						free_dir:
							free(files_buffer);
							free(dir->Include);
							free(dir->Exclude);
							free(dir->Recursive);
							free(dir->Path);
					}
					else
					{
						enum TagType tag = Manifest;
						WriteMeasurementFile(line, NULL, handle1, ioStatusBlock1, tag);
					}
					free(line);
					break;
				}
			}
			else if (ntstatus == STATUS_END_OF_FILE)
			{
				line[i] = '\0';
				DbgPrint("%s\n", line);
				enum TagType tag = Manifest;
				WriteMeasurementFile(line, NULL, handle1, ioStatusBlock1, tag);
				free(line);
				break;
			}
			else {
				free(line);
				break;
			}
		}
	} while (NT_SUCCESS(ntstatus) || ntstatus != STATUS_END_OF_FILE);

	ZwClose(handle);
	ZwClose(handle1);

	RtlStringCbCopyA(hash_file, sizeof(hash_file), fs_root_path);
	RtlStringCbCatA(hash_file, sizeof(hash_file), "C:\\Windows\\Logs\\MeasuredBoot\\measurement.");
	RtlStringCbCatA(hash_file, sizeof(hash_file), hashType);
	DbgPrint("Measurement file is %s\n", hash_file);

	RtlInitAnsiString(&ntName, hash_file);
	RtlAnsiStringToUnicodeString(&uniName, &ntName, TRUE);
	InitializeObjectAttributes(&objAttr, &uniName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL, NULL);

	ntstatus = ZwCreateFile(&handle1,
		GENERIC_WRITE,
		&objAttr, &ioStatusBlock1,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		0,
		FILE_OVERWRITE_IF,
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL, 0);
	DbgPrint("ZwCreateFile returns : 0x%x\n", ntstatus);

	bin2hex(cH, strnlen_s(cH, MAX_HASH_LEN), cH2, sizeof(cH2));
	DbgPrint("Hash Measured : %s\n", cH2);
	WriteMeasurementFile(NULL, cH2, handle1, ioStatusBlock1, Manifest);
	ZwClose(handle1);

	///* extend to PCR 16 */
	#define TAG_SIZE 20
	////#define TAG_PCR 16
	BYTE assetTag[TAG_SIZE] = "97fc38c88a9c160c84b6";
	BYTE newPCRV[TAG_SIZE] = { 0 };
	TpmPCRExtend(15, assetTag, newPCRV);
	TpmPCRExtend(16, assetTag, newPCRV);
	TpmPCRExtend(23, assetTag, newPCRV);

	ma_status = 1;
}

/*
 * DriverEntry funtion.
 * Entry point function for driver.
 */
NTSTATUS
DriverEntry(
IN PDRIVER_OBJECT pDriverObject,
IN PUNICODE_STRING pusUnicodeString)
{
	DbgPrint("In the driver entry function.\n");
	ULONG ulIndex;
	PDRIVER_DISPATCH *dispatch;

	UNREFERENCED_PARAMETER(pusUnicodeString);

	for (ulIndex = 0, dispatch = pDriverObject->MajorFunction;
		ulIndex <= IRP_MJ_MAXIMUM_FUNCTION;
		ulIndex++, dispatch++)
	{
		DbgPrint("Initializing the dispatch routines = %d\n", ulIndex);
		*dispatch = tbootdriverSendToNextDriver;
	}
	
	DbgPrint("tbootdriverCreate assigned.\n");
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = tbootdriverCreate;

	DbgPrint("tbootdriverAddDevice assigned.\n");
	pDriverObject->DriverExtension->AddDevice = tbootdriverAddDevice;

	DbgPrint("Unload routine assigned.\n");
	pDriverObject->DriverUnload = tbootdriverUnload;

	DbgPrint("Returning from driver entry.\n");
	return(STATUS_SUCCESS);
}

void 
tbootdriverUnload(
IN PDRIVER_OBJECT pDriverObject
)
{
	PAGED_CODE();

	UNREFERENCED_PARAMETER(pDriverObject);

	return;
}

NTSTATUS
tbootdriverCreate(
IN PDEVICE_OBJECT pDeviceObject,
IN PIRP pIrp)
{
	PAGED_CODE();

	if (NULL == pDeviceObject || NULL == pIrp)
	{
		DbgPrint("Invalid Parameters.\n");
		return STATUS_INVALID_PARAMETER;
	}

	UNREFERENCED_PARAMETER(pDeviceObject);

	doMeasurement();

	pIrp->IoStatus.Status = STATUS_SUCCESS;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS
tbootdriverAddDevice(
IN PDRIVER_OBJECT pDriverObject,
IN PDEVICE_OBJECT pPhysicalDeviceObject)
{
	DbgPrint("In Add Device routine.\n");
	NTSTATUS status;
	PDEVICE_OBJECT  filterDeviceObject;
	PDEVICE_EXTENSION deviceExtension;

	PAGED_CODE();

	DbgPrint("Io creating Device.\n");
	status = IoCreateDevice(pDriverObject,
		DEVICE_EXTENSION_SIZE,
		NULL,
		FILE_DEVICE_DISK,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&filterDeviceObject
		);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("tbootdriverAddDevice:: IoCreateDevice failed.\n");
		return status;
	}

	DbgPrint("Device Created assigning flags.\n");

	filterDeviceObject->Flags |= DO_DIRECT_IO;

	deviceExtension = (PDEVICE_EXTENSION)filterDeviceObject->DeviceExtension;

	DbgPrint("Zeroing memory.\n");
	RtlZeroMemory(deviceExtension, DEVICE_EXTENSION_SIZE);

	deviceExtension->PhysicalDeviceObject = pPhysicalDeviceObject;

	DbgPrint("Attaching device to device stack.\n");
	deviceExtension->TargetDeviceObject = IoAttachDeviceToDeviceStack(filterDeviceObject, pPhysicalDeviceObject);

	if (deviceExtension->TargetDeviceObject == NULL)
	{
		ExFreePool(deviceExtension->DiskCounters);
		deviceExtension->DiskCounters = NULL;
		IoDeleteDevice(filterDeviceObject);
		DbgPrint("tbootdriverAddDevice::Failed to attach to device stack.\n");
		return STATUS_NO_SUCH_DEVICE;
	}
	
	deviceExtension->DeviceObject = filterDeviceObject;

	deviceExtension->PhysicalDeviceName.Buffer
		= deviceExtension->PhysicalDeviceNameBuffer;

	DbgPrint("KeInitializing Event.\n");
	KeInitializeEvent(&deviceExtension->PagingPathCountEvent,
		NotificationEvent, TRUE);

	filterDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	DbgPrint("returning from Add Device.\n");
	return STATUS_SUCCESS;
}

NTSTATUS
tbootdriverSendToNextDriver(
IN PDEVICE_OBJECT pDeviceObject,
IN PIRP Irp)
{
	PDEVICE_EXTENSION pDeviceExtension;

	IoSkipCurrentIrpStackLocation(Irp);

	pDeviceExtension = (PDEVICE_EXTENSION)pDeviceObject->DeviceExtension;

	return IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
}