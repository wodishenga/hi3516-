/*
 * Copyright (C) 2013 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/dma-buf.h>
#include <linux/highmem.h>
#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/version.h>

struct hifb_memblock_pdata {
    phys_addr_t base;
};

static struct sg_table *hifb_memblock_map(struct dma_buf_attachment *attach,
        enum dma_data_direction direction)
{
    struct hifb_memblock_pdata *pdata = attach->dmabuf->priv;
    unsigned long pfn = PFN_DOWN(pdata->base);
    struct page *page = pfn_to_page(pfn);
    struct sg_table *table;
    struct scatterlist *sg;
    int i;
    int ret;
    table = kzalloc(sizeof(*table), GFP_KERNEL);
    if (!table)
        return ERR_PTR(-ENOMEM);

    ret = sg_alloc_table(table, 1, GFP_KERNEL);
    if (ret < 0)
        goto err;
 
    sg_set_page(table->sgl, page, attach->dmabuf->size, 0);
        for_each_sg(table->sgl, sg, table->nents, i)
            if(sg)
            {
                sg_dma_address(sg) = sg_phys(sg);
            }
    return table;

err:
    kfree(table);
    return ERR_PTR(ret);
}

static void hifb_memblock_unmap(struct dma_buf_attachment *attach,
        struct sg_table *table, enum dma_data_direction direction)
{
    sg_free_table(table);
}

static void __init_memblock hifb_memblock_release(struct dma_buf *buf)
{
    // dead code. 3lines commented with "//" by y00379198 2017.4.27
    //struct hifb_memblock_pdata *pdata = buf->priv;
    /*int err = memblock_free(pdata->base, buf->size);*/
    //int err = 1;
    //if (err < 0)
    //kfree(pdata);
}

static void *hifb_memblock_do_kmap(struct dma_buf *buf, unsigned long pgoffset,
        bool atomic)
{
    struct hifb_memblock_pdata *pdata = buf->priv;
    unsigned long pfn = PFN_DOWN(pdata->base) + pgoffset;
    struct page *page = pfn_to_page(pfn);

    if (atomic)
        return kmap_atomic(page);
    else
        return kmap(page);
}

static void *hifb_memblock_kmap_atomic(struct dma_buf *buf,
        unsigned long pgoffset)
{
    return hifb_memblock_do_kmap(buf, pgoffset, true);
}

static void hifb_memblock_kunmap_atomic(struct dma_buf *buf,
        unsigned long pgoffset, void *vaddr)
{
    kunmap_atomic(vaddr);
}

static void *hifb_memblock_kmap(struct dma_buf *buf, unsigned long pgoffset)
{
    return hifb_memblock_do_kmap(buf, pgoffset, false);
}

static void hifb_memblock_kunmap(struct dma_buf *buf, unsigned long pgoffset,
        void *vaddr)
{   
    kunmap(vaddr);
}

static int hifb_memblock_mmap(struct dma_buf *buf, struct vm_area_struct *vma)
{
    struct hifb_memblock_pdata *pdata = buf->priv;

    return remap_pfn_range(vma, vma->vm_start, PFN_DOWN(pdata->base),
            vma->vm_end - vma->vm_start, vma->vm_page_prot);
}


struct dma_buf_ops hifb_memblock_ops = {
    .map_dma_buf = hifb_memblock_map,
    .unmap_dma_buf = hifb_memblock_unmap,
    .release = hifb_memblock_release,
    .kmap_atomic = hifb_memblock_kmap_atomic,
    .kunmap_atomic = hifb_memblock_kunmap_atomic,
    .kmap = hifb_memblock_kmap,
    .kunmap = hifb_memblock_kunmap,
    .mmap = hifb_memblock_mmap,
};

/**
 * hifb_memblock_export - export a memblock reserved area as a dma-buf
 *
 * @base: base physical address
 * @size: memblock size
 * @flags: mode flags for the dma-buf's file
 *
 * @base and @size must be page-aligned.
 *
 * Returns a dma-buf on success or ERR_PTR(-errno) on failure.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,0,0)
struct dma_buf *hifb_memblock_export(phys_addr_t base, size_t size, int flags)
{
    struct hifb_memblock_pdata *pdata;
    struct dma_buf *buf;

    if (PAGE_ALIGN(base) != base || PAGE_ALIGN(size) != size)
        return ERR_PTR(-EINVAL);

    pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);

    if (!pdata)
         return ERR_PTR(-ENOMEM);

    pdata->base = base;
    buf = dma_buf_export(pdata, &hifb_memblock_ops, size, flags, NULL);
    if (IS_ERR(buf))
        kfree(pdata);

    return buf;
}
    
#else

struct dma_buf *hifb_memblock_export(phys_addr_t base, size_t size, int flags)
{
    struct dma_buf_export_info * exp_info;
    struct dma_buf *buf;

    if (PAGE_ALIGN(base) != base || PAGE_ALIGN(size) != size)
        return ERR_PTR(-EINVAL);

    exp_info = kzalloc(sizeof(*exp_info), GFP_KERNEL);

    if (!exp_info)
        return ERR_PTR(-ENOMEM);

    exp_info->exp_name = "exp_name";
    exp_info->owner = THIS_MODULE;
    exp_info->ops = &hifb_memblock_ops;
    exp_info->size = size;
    exp_info->flags = flags;
    exp_info->resv = NULL;
    exp_info->priv = (void *)base;

    buf = dma_buf_export(exp_info);
    if (IS_ERR(buf))
        kfree(exp_info);

    return buf;

}
#endif

