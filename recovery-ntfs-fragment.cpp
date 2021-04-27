DWORD WINAPI CUndeleteDlg::ScanFilesThread(LPVOID lpVoid)
{
	log_NOTICE(L"Scan file t 0");
	// Install exception handlers for this thread
	crInstallToCurrentThread2(0);
	log_NOTICE(L"Scan file t");

	CUndeleteDlg* pDlg = (CUndeleteDlg*) lpVoid;

	CString firstLetterDrive;

	pDlg->isSavedScanInfo = true;

	map<wstring, XFILEINFO *>::iterator iter_l2;

	typedef struct ParentDir_Struct
	{
		LONGLONG num; // имя текущей директории
		wstring foldName;// ссылка на директорию, которая уровнем выше
	} ParentDir;
	// Кеш для папок: дочерняя, родительская

	map<LONGLONG, ParentDir>numberMftDirCache;

	CString dTotalFiles;
	CString totFiles = GetTranslatedStr(_TEXT("Total files"));
	CString dFiles;
	CString delFiles = GetTranslatedStr(_TEXT("Deleted files"));

	log_NOTICE(L"Del all it");
	pDlg->m_Tree.DeleteAllItems();
	pDlg->m_listCtrl.DeleteAllItems();

	pDlg->m_stat.SetPaneText(0, _TEXT(""));
	pDlg->m_stat.SetPaneText(1, _TEXT(""));
	pDlg->m_stat.SetPaneText(2, _TEXT(""));

	pDlg->enableFindFirstButton = true;
	pDlg->enablePreviousButton = true;
	pDlg->enableNextButton = true;
	pDlg->m_wndTBar.SetButtonStyle(FIND_BUTTON, TBBS_BUTTON);
	pDlg->m_wndTBar.SetButtonStyle(FIND_PREVIOUS_BUTTON, TBBS_BUTTON);
	pDlg->m_wndTBar.SetButtonStyle(FIND_NEXT_BUTTON, TBBS_BUTTON);
	pDlg->enableScanButton = false;
	pDlg->m_wndTBar.SetButtonStyle(SCAN_BUTTON, TBBS_DISABLED);
	pDlg->m_wndTBar.SetButtonStyle(FULL_SCAN_BUTTON, TBBS_DISABLED);
	pDlg->m_wndTBar.SetButtonStyle(ADVANCED_SCAN_BUTTON, TBBS_DISABLED);
	pDlg->enableRecoverButton = false;
	pDlg->enablePreview = false;
	pDlg->enableStopButton = true;
	pDlg->m_wndTBar.SetButtonStyle(STOP_BUTTON, TBBS_BUTTON);
	pDlg->enableSaveScanInfoButton = false;
	pDlg->enableSaveListButton = true;

	DRIVEPACKET* pDrive;	
	CNTFSDrive::ST_FILEINFO stFInfo;
	DWORD i = 0;
	DWORD nRet;
	HTREEITEM hTreeItem;

	char* lpMsgBuf;
	CTreeCtrl* pcTree = (CTreeCtrl*) pDlg->GetDlgItem(IDC_TREDISKS);

	log_NOTICE(L"Get sel it");
	hTreeItem = pcTree->GetSelectedItem();
 
	driveStr = pcTree->GetItemText(hTreeItem);
	CString driveLetter;
	CString myComp = GetTranslatedStr(_T("My computer"));
	if (0 != myComp.Compare(driveStr))
	{
		driveLetter = driveStr;
		log_NOTICE(L"drv letter", driveLetter);
	}
	else 
	{
		log_NOTICE(L"Select logical drive and press Scan button");
		pDlg->MessageBox((LPTSTR) GetTranslatedStr(_TEXT("Select logical drive and press Scan button")).GetBuffer(),
			GetTranslatedStr(_TEXT("Select logical drive and press Scan button")).GetBuffer(), MB_OK);
		goto exitThread;
	}

	// Если true, то сканируем не только удаленные файлы, но и существующие

	bool isSearchAllFiles = false;
	if (_TEXT(' ') == driveStr[0]) //" Disk 1"
	{
		log_NOTICE(L"is All Files = true");
		isSearchAllFiles = true;
	}

	pDrive = (DRIVEPACKET *) pcTree->GetItemData(hTreeItem);
	if (!pDrive)
	{
		log_NOTICE(L"exit thread1");
		goto exitThread;
	}

	XFILEINFO* pRoot;
	XFILEINFO* pRootLostFat;

	if (_TEXT(' ') != driveStr[0])
	{
		firstLetterDrive.Append(driveStr, 2);
	}
	else
	{
		firstLetterDrive.Append(driveStr);
	}
	log_NOTICE(L"let drv: ", firstLetterDrive);
	
	pRootLostFat = new XFILEINFO(firstLetterDrive.GetBuffer());// только первый символ диска

	pRootLostFat->sPath = firstLetterDrive.GetBuffer();
	pRootLostFat->bIsDir = TRUE;

	pRoot = new XFILEINFO(firstLetterDrive.GetBuffer());
	pRoot->sPath = firstLetterDrive.GetBuffer();
	pRoot->bIsDir = TRUE;

	log_NOTICE(L"add r");
	pDlg->pHyperRoot->AddFile(pRoot);

	if (XFS_FS == pDrive->fileSystem)
	{
		log_NOTICE(L"xfs start");

		TCHAR physDrive[255] = {_TEXT('\0')};

		if (-1 == pDrive->driveNumber)
		{
			wsprintf(physDrive, _TEXT("\\\\.\\%c:"), (TCHAR)driveStr[0]);
		}
		else
		{
			wsprintf(physDrive, _TEXT("\\\\.\\PhysicalDrive%d"), pDrive->driveNumber);
		}

		pDlg->cDrive.dwNumSectors = pDrive->dwNumSectors; // размер в секторах

		pDlg->cDrive.dwNTRelativeSector = pDrive->dwNTRelativeSector; // смещение от начала диска

		pDlg->cDrive.dwBytesPerSector = pDrive->dwBytesPerSector;
		pDlg->cDrive.driveNumber = pDrive->driveNumber;
		pDlg->cDrive.fileSystem = pDrive->fileSystem;

		//XFS
		int type = 7;


		DWORD startSector = pDlg->cDrive.dwNTRelativeSector;
		DWORD volSize = pDlg->cDrive.dwNumSectors;

		log_NOTICE(L"init XFS");
		xfsRecov.Init(&physDrive[0], startSector, type, volSize);

		int multiple = 1;
		if (-1 == pDlg->cDrive.driveNumber)
		{
			multiple = pDrive->dwBytesPerSector / 512;
		}

		XFILEINFO* pCurr2 = pRoot;

		xfsRecov.SetMultiple(multiple);

		x_entry.clear();

		if (FULL_SCAN == pDlg->fullScan)
		{
			log_NOTICE(L"scan ex XFS");
			pDlg->ScanExistingXFS(&xfsRecov, pDlg->cDrive.dwNumSectors);
		}
		else
		{
			log_NOTICE(L"scan XFS");
			pDlg->ScanXFS(&xfsRecov, pDlg->cDrive.dwNumSectors);
		}

		entryMap.clear();
		// Получаю карту записей(включает в себя файлы и папки). Ключ - номер файла. Значение - родительский каталог.
		for (vector<FileEntry>::iterator it = x_entry.begin(); it != x_entry.end(); it++)
		{
					entryMap.insert(entryMapType(it->FileId, it->ParentId));
		}

		dirMap.clear();
		// Добавляю папки

		for (vector<FileEntry>::iterator it = x_entry.begin(); it != x_entry.end(); it++)
		{
			if (2 == it->FileType)
			{
				log_NOTICE(L"Add Folder");
				AddFolder(pCurr2, it);
			}
		}

		// Добавляю файлы

		for (vector<FileEntry>::iterator it = x_entry.begin(); it != x_entry.end(); it++)
		{			
			if (3 == it->FileType)
			{

				map<unsigned int, XFILEINFO *>::iterator iter;
				iter = dirMap.find( it->ParentId );

				// Если файл не в папке, то добавляем их в корень

				if (dirMap.end() == iter)
				{
					XFILEINFO *pXfsFile = new XFILEINFO((LPWSTR) CT2W(it->Name));
					pXfsFile->AddInfo(it->Length, it->CreateTime, it->UpdateTime, it->FileId, it->Attribute, _T(""));
					pXfsFile->bIsDir = false;
					log_NOTICE(it->Name);
					pCurr2->AddFile(pXfsFile);
				}
				// Если файл в папке, то добавляем их в эту папку

				else
				{
					XFILEINFO *pXfsFile = new XFILEINFO((LPWSTR) CT2W(it->Name));
					pXfsFile->AddInfo(it->Length, it->CreateTime, it->UpdateTime, it->FileId, it->Attribute, _T(""));
					pXfsFile->bIsDir = false;
					XFILEINFO *pCurr2Dir = (*iter).second;
					log_NOTICE(it->Name);
					pCurr2Dir->AddFile(pXfsFile);
				}
			}
		}

		log_NOTICE(L"exit2");
		goto exitThread;
	}


	if ((HFS_HFSPLUS_FS == pDrive->fileSystem) || (HFS_HFS_FS == pDrive->fileSystem))
	{
		log_NOTICE(L"HFS start");
		TCHAR physDrive[255] = {_TEXT('\0')};

		if (-1 == pDrive->driveNumber)
		{
			wsprintf(physDrive, _TEXT("\\\\.\\%c:"), (TCHAR)driveStr[0]);
		}
		else
		{
			wsprintf(physDrive, _TEXT("\\\\.\\PhysicalDrive%d"), pDrive->driveNumber);
		}

		pDlg->cDrive.dwNumSectors = pDrive->dwNumSectors; // размер в секторах

		pDlg->cDrive.dwNTRelativeSector = pDrive->dwNTRelativeSector; // смещение от начала диска

		pDlg->cDrive.dwBytesPerSector = pDrive->dwBytesPerSector;
		pDlg->cDrive.driveNumber = pDrive->driveNumber;
		pDlg->cDrive.fileSystem = pDrive->fileSystem;

		log_NOTICE(L"scan vol");
		scanVolume(hTreeItem, pDlg->cDrive.fileSystem, pDrive, physDrive);
		
		STARTUPINFO si = { sizeof(si) };
		PROCESS_INFORMATION pi;

		TCHAR pathLog[_MAX_PATH] = {'\0'};
		GetPathName(pathLog, sizeof(pathLog), L"\\recovery.exe");

		log_NOTICE(L"create pr HFS");
		CreateProcess(NULL, pathLog, NULL, NULL,
			FALSE, 0, NULL, NULL, &si, &pi);

		log_NOTICE(L"exit3");
		goto exitThread;
	}


	TCHAR physDrive[255] = {_TEXT('\0')};
	log_NOTICE(L"drv numb: ", integer(pDrive->driveNumber));
	wsprintf(physDrive, _TEXT("\\\\.\\PhysicalDrive%d"), pDrive->driveNumber);

	pDlg->cDrive.wCylinder = pDrive->wCylinder;
	pDlg->cDrive.wHead = pDrive->wHead;
	pDlg->cDrive.wSector = pDrive->wSector;
	pDlg->cDrive.dwNumSectors = pDrive->dwNumSectors;
	pDlg->cDrive.wType = pDrive->wType;
	pDlg->cDrive.dwRelativeSector = pDrive->dwRelativeSector;
	pDlg->cDrive.dwNTRelativeSector = pDrive->dwNTRelativeSector;
	pDlg->cDrive.dwBytesPerSector = pDrive->dwBytesPerSector;
	pDlg->cDrive.driveNumber = pDrive->driveNumber;
	pDlg->cDrive.fileSystem = pDrive->fileSystem;


	if ((pDlg->cDrive.fileSystem == FAT_12 
		|| pDlg->cDrive.fileSystem == FAT_16
		|| pDlg->cDrive.fileSystem == FAT_32) && (pDlg->cDrive.fileSystem == 0x42))
	{
		log_NOTICE(L"FAT start");
		FirstFatByte = 0;
		pDlg->cDrive.dwNTRelativeSector = 0;
		_tcscpy_s(physDrive, _TEXT("\\\\.\\"));

		if (_TEXT(' ') != driveStr[0])
		{
			_tcsncat(physDrive, driveStr.GetBuffer(2), 2);
			log_NOTICE(L"ph d: ", physDrive);
		}
		else
		{
			_tcscat(physDrive, driveStr.GetBuffer());
			log_NOTICE(L"ph d2: ", physDrive);
		}
	}


	if (pDlg->cDrive.fileSystem == NTFSFS) 
	{
		log_NOTICE(L"NTFS start");
		pDlg->m_Tree.ShowWindow(SW_SHOWNORMAL);
		pDlg->m_listCtrl.ShowWindow(SW_SHOWNORMAL);

		//- Тут возможно неправильно закрывается хендел m_hDrive
		pDlg->m_hDrive = CreateFile(physDrive, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (pDlg->m_hDrive == INVALID_HANDLE_VALUE) {
			DWORD err = GetLastError();

			if (ERROR_ACCESS_DENIED == err)
			{
				log_ERROR(L"You do not have Administration privileges on this computer");
				pDlg->MessageBox((LPTSTR) GetTranslatedStr(_TEXT("You do not have Administration privileges on this computer to start this program.\n\
																 After start Windows enter a user name and password of any user with Administrator privileges.")).GetBuffer(), _TEXT(""), MB_OK | MB_ICONWARNING);
			}
			else
			{
				log_ERROR(L"Error ", integer(GetLastError()));
				FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL);
				pDlg->MessageBox((LPTSTR) lpMsgBuf, _TEXT("Error in CreateFile (scan)"), MB_OK | MB_ICONERROR);
				LocalFree(lpMsgBuf);
				CloseHandle(pDlg->m_hDrive);
			}
			log_NOTICE(L"exit4");
			goto exitThread;
		}

		log_NOTICE(L"Set Drv Info ");
		pDlg->m_cNTFS.SetDriveHandle(pDlg->m_hDrive); // set the physical drive handle
		pDlg->m_cNTFS.SetStartSector(pDlg->cDrive.dwNTRelativeSector, 512); // set the starting sector of the NTFS

		log_NOTICE(L"Init Mini");
		// Считываем данные о файле MFT
		nRet = pDlg->m_cNTFS.InitializeMini(); // initialize, ie. read all MFT in to the memory
		if (nRet) {
			log_ERROR(L"Error ", integer(nRet));
			CloseHandle(pDlg->m_hDrive);
			pDlg->m_hDrive = INVALID_HANDLE_VALUE;
			log_NOTICE(L"exit5");
			goto exitThread;
		}

		if (0 >= pDlg->m_cNTFS.m_dwBytesPerCluster)
		{
			log_ERROR(L"Error: BytesPerCluster is zero");
			pDlg->MessageBox( NULL, _TEXT("Error: BytesPerCluster is zero"), MB_OK | MB_ICONERROR );
			pDlg->m_cNTFS.m_dwBytesPerCluster = 4096;
		}

	}


	if (pDlg->cDrive.fileSystem == FAT_12 
		|| pDlg->cDrive.fileSystem == FAT_16
		|| pDlg->cDrive.fileSystem == FAT_32)
	{
		log_NOTICE(L"FAT start2");
		pDlg->m_Tree.ShowWindow(SW_SHOWNORMAL);
		pDlg->m_listCtrl.ShowWindow(SW_SHOWNORMAL);

		LARGE_INTEGER n84StartPos;
		log_NOTICE(L"create f FAT");
		hDrv16 = CreateFile(physDrive, GENERIC_READ,  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
		n84StartPos.QuadPart = (LONGLONG) 512 * pDlg->cDrive.dwNTRelativeSector;

		FirstFatByte = n84StartPos.QuadPart;
		nRet = SetFilePointer(hDrv16, n84StartPos.LowPart, &n84StartPos.HighPart, FILE_BEGIN);
		log_NOTICE(L"sf12: ", integer(nRet));
		GetBoot();
		log_NOTICE(L"FAT end2");
	}


	///////////////////

	XFILEINFO* pCurr = pRoot;

	TCHAR phDrive[255] = {_TEXT('\0')};
	wsprintf(phDrive, _TEXT("%c:\\"), (TCHAR)driveStr[0]);
	log_NOTICE(L"Load Ind L");
	LoadIndexList(&phDrive[0]);
	log_NOTICE(L"Load Info L");
	LoadFileInfoList(&phDrive[0]);

	PFILEINFO pfi;
	POSITION pos;

	log_NOTICE(L"Get head pos");
	pos = filist.GetHeadPosition();

	// для виртуальной корзины

	// находим полный путь

	while (pos != NULL)
	{
		log_NOTICE(L"Get n");
		pfi = filist.GetNext(pos);
		if (TYPE_FOLDER == pfi->ii->type)
		{
			log_NOTICE(L"type fold continue");
			continue;
		}

		if (pfi != NULL)
		{
			XFILEINFO* pFile = new XFILEINFO(pfi->fname);

			if (FS_FAT == pfi->fs_type)
			{
				pFile->AddInfoSize(pfi->fi.fatfi.entry8_3.filesize);

				FILETIME ftc;
				DosDateTimeToFileTime(pfi->fi.fatfi.entry8_3.cdate, pfi->fi.fatfi.entry8_3.ctime, &ftc);
				pFile->AddInfoCreateTime(*((LONGLONG *)(&ftc)));
				FILETIME ftf;
				DosDateTimeToFileTime(pfi->fi.fatfi.entry8_3.fdate, pfi->fi.fatfi.entry8_3.ftime, &ftf);
				pFile->AddInfoModifyTime(*((LONGLONG *)(&ftf)));

				pCurr = pRoot;

				if (0 != pfi->ii->pid)
				{
					wstring pathDir2;
					bool foldSearch = true;
					list<wstring> lPath;
					ULONGLONG u_id = pfi->ii->pid;
					ULONGLONG u_time = pfi->ii->pctime;
					while (true == foldSearch)
					{
						
						foldSearch = false;
						//
						PFILEINFO pfi2;
						POSITION pos2;

						log_NOTICE(L"Get H P");
						pos2 = filist.GetHeadPosition();
						while (pos2 != NULL)
						{
							pfi2 = filist.GetNext(pos2);
							if (pfi2 != NULL)
							{
								if (TYPE_FOLDER == pfi2->ii->type)
								{
									// если нашли родительскую папку

									if ((u_id == pfi2->ii->id) && (u_time == pfi2->ii->ctime))
									{
										log_NOTICE(L"is find");
										wstring pathDir;
										pathDir.assign(pfi2->fname);
										pathDir.append(_TEXT("\\"));
										pathDir.append(pathDir2);
										pathDir2.assign(pathDir);

										wstring cStr = pfi2->fname;
										log_NOTICE(L"push back", cStr);
										lPath.push_back(cStr);

										u_id = pfi2->ii->pid;
										u_time = pfi2->ii->pctime;

										foldSearch = true;
										break;
									}
								}
							}
						}
						if (0 == pfi2->ii->pid)
							break;
					}

					list<wstring>::iterator p;
					p = lPath.end();

					pCurr = pRoot;

					// Проходимся по пути в дереве папок, в котором должен храниться файл. Если какие-то папки не созданы, то создаем их.
					while (p != lPath.begin())
					{
						p--;

						// Текущее имя папки в пути файла. Например имя "old" в пути "C:\Undelete\new\old\Undel"
						wstring strDir = *p;
						bool existFolder = false;
						XFILEINFO * sinf;

						for (XFILELIST::iterator it = pCurr->lstFiles.begin();it != pCurr->lstFiles.end();it++)
						{
							wstring strList = (*it)->sName;
							if (strList == strDir)
							{
								sinf = (*it);
								existFolder = true;
								break;
							}
						}

						if (false == existFolder)
						{
							XFILEINFO* pDir1 = new XFILEINFO(strDir);
							pDir1->bIsDir = TRUE;
							pCurr->AddFile(pDir1);

							pCurr = pDir1;
						}
						else
						{
							pCurr = sinf;
						}
					}

					CString pathStr;
					pathStr.Append(phDrive);
					pathStr.Append(CW2T(pathDir2.c_str()));
					pFile->AddInfoPathFile((LPWSTR)CT2W(pathStr));

					
				}
				else
				{
					CString pathStr;
					pathStr.Append(phDrive);
					pFile->AddInfoPathFile((LPWSTR)CT2W(pathStr));
				}
				
			}
			else if (FS_NTFS == pfi->fs_type)
			{
				pFile->AddInfoSize(pfi->fi.ntfsfi.filesize);
				FILETIME ftc;
				DosDateTimeToFileTime(pfi->fi.ntfsfi.cdate, pfi->fi.ntfsfi.ctime, &ftc);
				pFile->AddInfoCreateTime(*((LONGLONG *)(&ftc)));
				FILETIME ftf;
				DosDateTimeToFileTime(pfi->fi.ntfsfi.fdate, pfi->fi.ntfsfi.ftime, &ftf);
				pFile->AddInfoModifyTime(*((LONGLONG *)(&ftf)));
				
				pFile->AddInfoAttributes(pfi->fi.ntfsfi.attribute);
			}

			if (FS_NTFS == pfi->fs_type)
			{
				// Берем адрес папки в которой содержится текущий файл

				LONGLONG numDir = pfi->ii->pid;
				wstring pathDir;
				wstring pathDir2;
				CNTFSDrive::ST_FILEINFO stFInfo2;

				bool fullPathFound = true;
				list<wstring> lPath;

				// формируем строку пути к текущему файлу

				while(0 != numDir)
				{
					map<LONGLONG, ParentDir>::const_iterator pos_n;
					pos_n = numberMftDirCache.find(numDir);
					if (numberMftDirCache.end() != pos_n)
					{
						stFInfo2.numberDir = pos_n->second.num;
						stFInfo2.szFilename = pos_n->second.foldName;
					}
					else {
						BYTE *recDir = new BYTE[4096];
						DWORD readSize = 4096;
						MFT_BLOCK myOff = pDlg->FindOffset(numDir);
						LONGLONG ost = fmodl(myOff.size, 4096);

						// считываем mft запись корневой папки

						nRet = pDlg->ReadRawMini(myOff.offset + (myOff.size - ost) / pDlg->m_cNTFS.m_dwBytesPerCluster, recDir, readSize, pDlg->m_cNTFS.m_dwBytesPerCluster, (LONGLONG)pDlg->m_cNTFS.m_dwBytesPerSector * pDlg->m_cNTFS.m_dwStartSector);
						if (nRet != 0)
						{
							delete [] recDir;
							goto exitThread;
						}

						// Получаем информацию о корневой папке

						nRet = pDlg->m_cNTFS.GetFileDetailMini(numDir, recDir + ost, stFInfo2, true); // get the file detail one by one 
						if ((nRet) && (nRet != ERROR_INVALID_PARAMETER))
						{
							delete [] recDir;
							goto exitThread;
						}

						delete [] recDir;

						ParentDir parDir;
						parDir.num = stFInfo2.numberDir;
						wstring strFN(stFInfo2.szFilename);
						parDir.foldName = strFN;

						numberMftDirCache.insert(make_pair(numDir, parDir));
					}

					// Номер папки = Считаннный номер корневой папки

					if (numDir == stFInfo2.numberDir)
					{
						break;
					}
					numDir = stFInfo2.numberDir;
					if (ERROR_PATH_NOT_FOUND == nRet)
					{
						fullPathFound = false;
					}
					// Если дошли до корневой папки, то выходим из цикла

					if ((0 == numDir) || (0 == stFInfo2.szFilename.compare(_TEXT("."))))
					{
						break;
					}

					pathDir.assign(stFInfo2.szFilename);
					pathDir.append(_TEXT("\\"));
					pathDir.append(pathDir2);
					pathDir2.assign(pathDir);


					wstring cStr = stFInfo2.szFilename;
					lPath.push_back(cStr);
				}

				// Докопируем к пути имя логического диска, если весь путь найдет. Иначе вместо буквы диска ставим "?\"
				if ((true == fullPathFound) && (0 == stFInfo2.szFilename.compare(_TEXT("."))))
				{
					pathDir.assign(driveLetter);
					pathDir.append(pathDir2);

					CString drLetter;
					drLetter.Append(driveLetter, 2);
					lPath.push_back(drLetter.GetBuffer());
				}
				else
				{
					pathDir.assign(_TEXT("?\\"));
					pathDir.append(pathDir2);

					lPath.push_back(_T("?"));
				}


				list<wstring>::iterator p;
				p = lPath.end();

				pCurr = pRoot;

				// Проходимся по пути в дереве папок, в котором должен храниться файл. Если какие-то папки не созданы, то создаем их.
				while (p != lPath.begin())
				{
					p--;

					// Текущее имя папки в пути файла."
					wstring strDir = *p;
					bool existFolder = false;
					XFILEINFO * sinf;

					for (XFILELIST::iterator it = pCurr->lstFiles.begin();it != pCurr->lstFiles.end();it++)
					{
						wstring strList = (*it)->sName;
						if (strList == strDir)
						{
							sinf = (*it);
							existFolder = true;
							break;
						}
					}

					if (false == existFolder)
					{
						XFILEINFO* pDir1 = new XFILEINFO(strDir);
						pDir1->bIsDir = TRUE;
						pCurr->AddFile(pDir1);

						pCurr = pDir1;
					}
					else
					{
						pCurr = sinf;
					}
				}

				// Каждому файлу добавленному в дерево запоминаем информацию об этом файле 
				CString pathStr;
				stFInfo.pathFile.assign(pathDir);
				pathStr.Append(CW2T(stFInfo.pathFile.c_str()));
				pFile->AddInfoPathFile((LPWSTR)CT2W(pathStr));

			}
			pFile->AddInfoNumber(pfi->ii->id);
			pCurr->AddFile(pFile);
		}
	}

	log_NOTICE(L"end virt bin");
	if (true == pDlg->isOpenVirtualBin)
	{
		log_NOTICE(L"exit6");
		goto exitThread;
	}
	///////////////////



	// Счетчик найденных удаленных файлов

	int countDeletedFiles = 0;

	if (pDlg->cDrive.fileSystem == FAT_12 
		|| pDlg->cDrive.fileSystem == FAT_16
		|| pDlg->cDrive.fileSystem == FAT_32)
	{
		log_NOTICE(L"FAT start3");

		ScanFat(pRoot, &pDlg->dlgProgress.isScan, &pDlg->m_bStopScanFilesThread);

		pRoot->AddFile(pRootLostFat);
		log_NOTICE(L"scan lost fat");
		ScanLostFat(pRootLostFat, &pDlg->dlgProgress, &pDlg->dlgProgress.isScan, &pDlg->m_bStopScanFilesThread);

		countDeletedFiles = countFATDeletedFiles;
		i = countFATTotalFiles;
		log_NOTICE(L"exit7");
		goto exitThread;
	}

	// Размер блока для считывания $mft. Считываем $mft файл большими блоками

	DWORD clastSize = pDlg->m_cNTFS.m_dwBytesPerCluster * 100;

	list<MFT_BLOCK>:: iterator bl = pDlg->m_cNTFS.NtfsBlock.begin();
	// Если пользователь выбрал полное сканирование, то расширяем область сканирования

	if (FULL_SCAN == pDlg->fullScan)
	{
		log_NOTICE(L"FULL SCAN");
		isSearchAllFiles = true;
	}

	// Расчитываем шаг ползунка прогресс бара

	bl = pDlg->m_cNTFS.NtfsBlock.begin();
	LONGLONG totalAnalizeFile = 0;
	LONGLONG mftSize = 1024;
	LONGLONG sizeBl = 0;
	while (bl != pDlg->m_cNTFS.NtfsBlock.end()) // перебираем run листы из $MFT файла, в которых содержаться записи mft
	{
		sizeBl = (*bl).size;
		totalAnalizeFile = totalAnalizeFile + sizeBl / mftSize;
		bl++;
	}
	LONGLONG stepCount = totalAnalizeFile / 50;
	if (0 == stepCount)
	{
		stepCount = 1;
	}
	LONGLONG progrCountOld = 0;
	bl = pDlg->m_cNTFS.NtfsBlock.begin();



	while (bl != pDlg->m_cNTFS.NtfsBlock.end()) // перебираем run листы из $MFT файла, в которых содержаться записи mft
	{
		if (pDlg->m_bStopScanFilesThread)
		{
			log_NOTICE(L"break SCAN");
			break;
		}
		if (false == pDlg->dlgProgress.GetScan())
		{
			log_NOTICE(L"break SCAN2");
			break;
		}

		// Cодержит смещение относительно начала диска и размер для текущего ран листа $MFT файла

		MFT_BLOCK mb;
		mb.offset = (*bl).offset;
		mb.size = (*bl).size;

		// Читаем $MFT таблицу большими блоками clastSize, т.к. на маленьких блоках уменьшается производительность

		for (LONGLONG off = 0; off < mb.size; off += clastSize)
		{			
			if (pDlg->m_bStopScanFilesThread)
			{
				log_NOTICE(L"break SCAN3");
				break;
			}
			if (false == pDlg->dlgProgress.GetScan())
			{
				log_NOTICE(L"break SCAN4");
				break;
			}

			// Содержит блок для считывания. Если мы считали почти все и остался остаток, то содержит размер этого остатока для считывания

			DWORD clastMassiveRead = clastSize;
			if ((off + clastSize) > mb.size)
			{
				clastMassiveRead = mb.size - off;
			}
			// Содержит блок mft записей

			BYTE *recData = new BYTE[clastMassiveRead];
			memset(recData, 0, clastMassiveRead);
			nRet = pDlg->ReadRawMini(mb.offset + off / pDlg->m_cNTFS.m_dwBytesPerCluster, recData, clastMassiveRead, pDlg->m_cNTFS.m_dwBytesPerCluster, (LONGLONG)pDlg->m_cNTFS.m_dwBytesPerSector * pDlg->m_cNTFS.m_dwStartSector);
			if (nRet != 0)
			{
				log_ERROR(L"RRM ret: ", integer(nRet));
				log_ERROR(L"exit 7");
				goto exitThread;
			}

			LONGLONG progrCount = i / stepCount;
			if (progrCount != progrCountOld)
			{
				pDlg->dlgProgress.SetProgress();
				progrCountOld = progrCount;
			}

			// Содержит количество mft записей в считанном блоке

			int sectMFT = (clastMassiveRead / 1024);

			// Перебираем записи mft в считанном блоке

			for (int k = 0; k < sectMFT; k++)
			{
				if (pDlg->m_bStopScanFilesThread)
				{
					log_NOTICE(L"break SCAN5");
					break;
				}

				if (false == pDlg->dlgProgress.GetScan())
				{
					log_NOTICE(L"break SCAN6");
					break;
				}

				// Получаем информацию о файле из mft записи

				nRet = pDlg->m_cNTFS.GetFileDetailMini(i + 0, recData + k * 1024, stFInfo, false, isSearchAllFiles);

				if (nRet == ERROR_NO_MORE_FILES)
				{
					log_ERROR(L"GetFDetailM: ", integer(nRet));
					delete [] recData;
					goto exitThread;
				}
				if (nRet == ERROR_INVALID_PARAMETER)
				{
					log_ERROR(L"GetFDetailM - ERROR_INVALID_PARAMETER ");
					i++;
					continue;
				}

				// Если сканируем поддиск "Disk 1", то выводим все файлы(удаленные и существующие) - isSearchAllFiles
				// удаленные файлы добавляем в дерево

				if (((true == stFInfo.bDeleted) || (true == isSearchAllFiles)) && (stFInfo.FileSize > 0)) 
				{
					// Берем адрес папки в которой содержится текущий файл

					LONGLONG numDir = stFInfo.numberDir;
					wstring pathDir;
					wstring pathDir2;
					CNTFSDrive::ST_FILEINFO stFInfo2;
					
					bool fullPathFound = true;
					list<wstring> lPath;

					// формируем строку пути к текущему файлу

					while(0 != numDir)
					{
						map<LONGLONG, ParentDir>::const_iterator pos_n;
						pos_n = numberMftDirCache.find(numDir);
						if (numberMftDirCache.end() != pos_n)
						{
							stFInfo2.numberDir = pos_n->second.num;
							stFInfo2.szFilename.assign(pos_n->second.foldName);
						}
						else {
 							BYTE *recDir = new BYTE[4096];
 							DWORD readSize = 4096;
							MFT_BLOCK myOff = pDlg->FindOffset(numDir);
 							LONGLONG ost = fmodl(myOff.size, 4096);
		
							// считываем mft запись корневой папки

							nRet = pDlg->ReadRawMini(myOff.offset + (myOff.size - ost) / pDlg->m_cNTFS.m_dwBytesPerCluster, recDir, readSize, pDlg->m_cNTFS.m_dwBytesPerCluster, (LONGLONG)pDlg->m_cNTFS.m_dwBytesPerSector * pDlg->m_cNTFS.m_dwStartSector);
							if (nRet != 0)
							{
								log_ERROR(L"RRM2: ", integer(nRet));
								delete [] recDir;
								goto exitThread;
							}

							// Получаем информацию о корневой папке

							nRet = pDlg->m_cNTFS.GetFileDetailMini(numDir, recDir + ost, stFInfo2, true); // get the file detail one by one 
								if ((nRet) && (nRet != ERROR_INVALID_PARAMETER))
							{
								log_ERROR(L"GetFDetailM2: ", integer(nRet));
								delete [] recDir;
								goto exitThread;
							}

							delete [] recDir;

							ParentDir parDir;
							parDir.num = stFInfo2.numberDir;
							wstring strFN(stFInfo2.szFilename);
							parDir.foldName = strFN;

							numberMftDirCache.insert(make_pair(numDir, parDir));
						}

						// Номер папки = Считаннный номер корневой папки

						if (numDir == stFInfo2.numberDir)
						{
							break;
						}
						numDir = stFInfo2.numberDir;

						if (ERROR_PATH_NOT_FOUND == nRet)
						{
							fullPathFound = false;
						}

						// Если дошли до корневой папки, то выходим из цикла

						if ((0 == numDir) || (0 == stFInfo2.szFilename.compare(_TEXT(".")) ))
						{
							break;
						}

						pathDir.assign(stFInfo2.szFilename);
						pathDir.append(_TEXT("\\"));
						pathDir.append(pathDir2);
						pathDir2.assign(pathDir);

						
						wstring cStr = stFInfo2.szFilename;
						lPath.push_back(cStr);
					}

					// Докопируем к пути имя логического диска, если весь путь найдет. Иначе вместо буквы диска ставим "?\"
					if ((true == fullPathFound) && (0 == stFInfo2.szFilename.compare(_TEXT("."))))
					{
						pathDir.assign(driveLetter);
						pathDir.append(pathDir2);

						CString drLetter;
						drLetter.Append(driveLetter, 2);
						lPath.push_back(drLetter.GetBuffer());
					}
					else
					{
						pathDir.assign(_TEXT("?\\"));
						pathDir.append(pathDir2);

						lPath.push_back(_T("?"));
					}


					list<wstring>::iterator p;
					p = lPath.end();

					XFILEINFO* pCurr = pRoot;


					// Проходимся по пути в дереве папок, в котором должен храниться файл. Если какие-то папки не созданы, то создаем их.
					while (p != lPath.begin())
					{
						p--;

						// Текущее имя папки в пути файла. Например имя "old" в пути "C:\Undelete\new\old\Undel"
						wstring strDir = *p;
						bool existFolder = false;
						XFILEINFO * sinf;

						for (XFILELIST::iterator it = pCurr->lstFiles.begin();it != pCurr->lstFiles.end();it++)
						{
							wstring strList = (*it)->sName;
							if (strList == strDir)
							{
								sinf = (*it);
								existFolder = true;
								break;
							}
						}
						
						if (false == existFolder)
						{
							XFILEINFO* pDir1 = new XFILEINFO(strDir);
							pDir1->bIsDir = TRUE;
							pCurr->AddFile(pDir1);

							pCurr = pDir1;
						}
						else
						{
							pCurr = sinf;
						}
					}

					// Каждому файлу добавленному в дерево запоминаем информацию об этом файле 
					stFInfo.pathFile.assign(pathDir);

					XFILEINFO* pFile = new XFILEINFO(stFInfo.szFilename);
					CString pathStr;
					pathStr.Append(CW2T(stFInfo.pathFile.c_str()));

					pFile->AddInfoSize(stFInfo.FileSize);
					pFile->AddInfoCreateTime(stFInfo.n64Create);
					pFile->AddInfoModifyTime(stFInfo.n64Modify);
					pFile->AddInfoNumber(stFInfo.fileNum);
					pFile->AddInfoAttributes(stFInfo.dwAttributes);
					// выдает ошибку

					pFile->AddInfoPathFile((LPWSTR) CT2W(pathStr));
					pCurr->AddFile(pFile);


					++countDeletedFiles;
				}

				i++;
			}
			delete [] recData;
		}
		bl++;
	}


exitThread : 
	log_NOTICE(L"-exitThread-", integer(nRet));
	scButton = false;

	pDlg->isOpenVirtualBin = false;

	log_NOTICE(L"SRF");
	pDlg->SetRootFile(pDlg->pHyperRoot, NULL);
	log_NOTICE(L"Upd Data");
	pDlg->UpdateData();

	pDlg->scanMap.insert(scanMapType(pRoot, pDrive));

 	dTotalFiles.Format(totFiles + _TEXT(": %i"), i);
 	pDlg->m_stat.SetPaneText(2, dTotalFiles);

 	dFiles.Format(GetTranslatedStr(delFiles + _TEXT(": %i")), countDeletedFiles);
 	pDlg->m_stat.SetPaneText(1, dFiles);

	pDlg->enableRecoverButton = true;
	pDlg->m_wndTBar.SetButtonStyle(RECOVER_BUTTON, TBBS_BUTTON);
	pDlg->enablePreview = true;
	pDlg->m_wndTBar.SetButtonStyle(PREVIEW_BUTTON, TBBS_BUTTON);
	pDlg->enableScanButton = true;
	pDlg->m_wndTBar.SetButtonStyle(SCAN_BUTTON, TBBS_BUTTON);
	pDlg->m_wndTBar.SetButtonStyle(FULL_SCAN_BUTTON, TBBS_BUTTON);
	pDlg->m_wndTBar.SetButtonStyle(ADVANCED_SCAN_BUTTON, TBBS_BUTTON);
	pDlg->enableStopButton = false;
	pDlg->m_wndTBar.SetButtonStyle(STOP_BUTTON, TBBS_DISABLED);

	pDlg->dlgProgress.ShowWindow(SW_HIDE);

	log_NOTICE(L"Sc F T");
	// Unset exception handlers before exiting the thread
	crUninstallFromCurrentThread();
	log_NOTICE(L"Sc F T0");

	return 0;
}